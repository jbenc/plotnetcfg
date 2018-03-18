/*
 * This file is a part of plotnetcfg, a tool to visualize network config.
 * Copyright (C) 2014 Red Hat, Inc. -- Jiri Benc <jbenc@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "netns.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "handler.h"
#include "if.h"
#include "list.h"
#include "master.h"
#include "match.h"
#include "netlink.h"
#include "sysfs.h"

#include "compat.h"

#define NETNS_RUN_DIR "/var/run/netns"

static long netns_get_kernel_id(const char *path)
{
	char dest[PATH_MAX], *s, *endptr;
	ssize_t len;
	long result;

	len = readlink(path, dest, sizeof(dest));
	if (len < 0)
		return -errno;
	dest[len] = 0;
	if (strncmp("net:[", dest, 5))
		return -EINVAL;
	s = strrchr(dest, ']');
	if (!s)
		return -EINVAL;
	*s = '\0';
	s = dest + 5;
	result = strtol(s, &endptr, 10);
	if (!*s || *endptr)
		return -EINVAL;
	return result;
}

static struct netns_entry *netns_check_duplicate(struct list *netns_list,
						 long int kernel_id)
{
	struct netns_entry *ns;

	list_for_each(ns, *netns_list)
		if (ns->kernel_id == kernel_id)
			return ns;
	return NULL;
}

static struct netns_entry *netns_create()
{
	struct netns_entry *ns;

	ns = calloc(1, sizeof(struct netns_entry));
	if (!ns)
		return NULL;

	list_init(&ns->ifaces);
	list_init(&ns->warnings);
	list_init(&ns->ids);

	return ns;
}

static int netns_get_var_entry(struct netns_entry **result,
			       struct list *netns_list,
			       const char *name)
{
	struct netns_entry *entry;
	char path[PATH_MAX];
	long kernel_id;
	int err;

	*result = entry = netns_create();
	if (!entry)
		return ENOMEM;

	snprintf(path, sizeof(path), "%s/%s", NETNS_RUN_DIR, name);
	entry->fd = open(path, O_RDONLY);
	if (entry->fd < 0)
		return errno;
	/* to get the kernel_id, we need to switch to that ns and examine
	 * /proc/self */
	err = netns_switch(entry);
	if (err)
		return err;
	kernel_id = netns_get_kernel_id("/proc/self/ns/net");
	if (kernel_id < 0)
		return -kernel_id;
	if (netns_check_duplicate(netns_list, kernel_id)) {
		close(entry->fd);
		free(entry);
		return -1;
	}
	entry->kernel_id = kernel_id;
	entry->name = strdup(name);
	if (!entry->name)
		return ENOMEM;
	return netns_switch_root();
}

static void netns_proc_entry_set_name(struct netns_entry *entry,
				      const char *spid)
{
	char path[PATH_MAX], buf[PATH_MAX];
	ssize_t len;
	int commfd;

	snprintf(path, sizeof(path), "/proc/%s/comm", spid);
	commfd = open(path, O_RDONLY);
	len = -1;
	if (commfd >= 0) {
		len = read(commfd, path, sizeof(path));
		if (len >= 0) {
			path[sizeof(path) - 1] = '\0';
			if (path[len - 1] == '\n')
				path[len - 1] = '\0';
		}
		close(commfd);
	}
	if (len >= 0)
		snprintf(buf, sizeof(buf), "PID %s (%s)", spid, path);
	else
		snprintf(buf, sizeof(buf), "PID %s", spid);
	entry->name = strdup(buf);
	if (!entry->name)
		entry->name = "?";
}

static int netns_get_proc_entry(struct netns_entry **result,
				struct list *netns_list,
				const char *spid)
{
	struct netns_entry *entry, *dup;
	char path[PATH_MAX];
	pid_t pid;
	long kernel_id;

	snprintf(path, sizeof(path), "/proc/%s/ns/net", spid);
	kernel_id = netns_get_kernel_id(path);
	if (kernel_id < 0) {
		/* ignore entries that cannot be read */
		return -1;
	}
	pid = atol(spid);
	dup = netns_check_duplicate(netns_list, kernel_id);
	if (dup) {
		if (dup->pid && (dup->pid > pid)) {
			dup->pid = pid;
			netns_proc_entry_set_name(dup, spid);
		}
		return -1;
	}

	*result = entry = netns_create();
	if (!entry)
		return ENOMEM;

	entry->kernel_id = kernel_id;
	entry->pid = pid;
	entry->fd = open(path, O_RDONLY);
	if (entry->fd < 0) {
		/* ignore entries that cannot be read */
		free(entry);
		return -1;
	}
	netns_proc_entry_set_name(entry, spid);
	return 0;
}

static int netns_new_list(struct list *result, int supported)
{
	struct netns_entry *root;

	root = netns_create();
	if (!root)
		return ENOMEM;
	if (supported) {
		root->kernel_id = netns_get_kernel_id("/proc/1/ns/net");
		if (root->kernel_id < 0)
			return -root->kernel_id;
		root->fd = open("/proc/1/ns/net", O_RDONLY);
		if (root->fd < 0)
			return errno;
	}

	list_init(result);
	list_append(result, node(root));
	return 0;
}

static int netns_add_var_list(struct list *netns_list)
{
	struct netns_entry *entry;
	struct dirent *de;
	DIR *dir;
	int err;

	dir = opendir(NETNS_RUN_DIR);
	if (!dir)
		return 0;

	while ((de = readdir(dir)) != NULL) {
		if (!strcmp(de->d_name, ".") ||
		    !strcmp(de->d_name, ".."))
			continue;
		err = netns_get_var_entry(&entry, netns_list, de->d_name);
		if (err < 0) {
			/* duplicate entry */
			continue;
		}
		if (err)
			return err;
		list_append(netns_list, node(entry));
	}
	closedir(dir);

	return 0;
}

static int netns_add_proc_list(struct list *netns_list)
{
	struct netns_entry *entry;
	struct dirent *de;
	DIR *dir;
	int err;

	dir = opendir("/proc");
	if (!dir)
		return 0;

	while ((de = readdir(dir)) != NULL) {
		if (!strcmp(de->d_name, ".") ||
		    !strcmp(de->d_name, ".."))
			continue;
		if (de->d_name[0] < '0' || de->d_name[0] > '9')
			continue;
		err = netns_get_proc_entry(&entry, netns_list, de->d_name);
		if (err < 0) {
			/* duplicate entry */
			continue;
		}
		if (err)
			return err;
		list_append(netns_list, node(entry));
	}
	closedir(dir);

	return 0;
}

/* Returns -1 if netnsids are not supported. */
static int netns_get_id(struct nl_handle *hnd, struct netns_entry *entry)
{
	struct nlmsg *req, *resp;
	int res = -1;

	req = rtnlmsg_new(RTM_GETNSID, AF_UNSPEC, 0, sizeof(struct rtgenmsg));
	if (!req)
		return -1;
	if (nla_put_u32(req, NETNSA_FD, entry->fd))
		goto out_req;
	if (nl_exchange(hnd, req, &resp))
		goto out_req;
	if (!nlmsg_get(resp, sizeof(struct rtgenmsg)))
		goto out_resp;
	for_each_nla(a, resp) {
		if (a->nla_type == NETNSA_NSID) {
			res = nla_read_s32(a);
			break;
		}
	}

out_resp:
	nlmsg_free(resp);
out_req:
	nlmsg_free(req);
	return res;
}

/* This is best effort only, if anything fails (e.g. netnsids are not
 * supported by kernel), we fail back to heuristics. */
static void netns_get_all_ids(struct netns_entry *current, struct list *netns_list)
{
	struct nl_handle hnd;
	struct netns_entry *entry;
	struct netns_id *nsid;
	int id;

	if (netns_switch(current))
		return;
	if (rtnl_open(&hnd) < 0)
		return;

	list_for_each(entry, *netns_list) {
		id = netns_get_id(&hnd, entry);
		if (id < 0)
			continue;
		nsid = malloc(sizeof(*nsid));
		if (!nsid)
			break;
		nsid->ns = entry;
		nsid->id = id;
		list_append(&current->ids, node(nsid));
	}
	nl_close(&hnd);
}

int netns_fill_list(struct list *result, int supported)
{
	struct netns_entry *entry;
	int err;

	err = netns_new_list(result, supported);
	if (err)
		return err;
	if (supported) {
		err = netns_add_var_list(result);
		if (err)
			return err;
		err = netns_add_proc_list(result);
		if (err)
			return err;
	}

	if ((err = sysfs_init()))
		return err;

	list_for_each(entry, *result) {
		if (entry->name) {
			/* Do not try to switch to the root netns, as we're
			 * already there when processing the first entry,
			 * and netns_switch fails hard if there's no netns
			 * support available. */
			if ((err = netns_switch(entry)))
				return err;
		}
		if ((err = sysfs_mount(entry->name)))
			return err;
		if ((err = if_list(&entry->ifaces, entry)))
			return err;
		if ((err = netns_handler_scan(entry)))
			return err;
		sysfs_umount();
	}
	/* Walk all net name spaces again and gather all kernel assigned
	 * netnsids. We don't assign netnsids ourselves to prevent assigning
	 * them needlessly - the kernel assigns only those that are really
	 * needed while doing netlinks dumps. Note also that netnsids are
	 * per name space and this is O(n^2). */
	list_for_each(entry, *result)
		netns_get_all_ids(entry, result);
	/* And finally, resolve netnsid+ifindex to the if_entry pointers. */
	match_all_netnsid(result);

	if ((err = master_resolve(result)))
		return err;
	if ((err = global_handler_post(result)))
		return err;
	if ((err = if_handler_post(result)))
		return err;
	return 0;
}

static int do_netns_switch(int fd)
{
	if (syscall(__NR_setns, fd, CLONE_NEWNET) < 0)
		return errno;
	return 0;
}

int netns_switch(struct netns_entry *dest)
{
	return do_netns_switch(dest->fd);
}

/* Used also to detect whether netns support is available.
 * Returns 0 if everything went okay, -1 if there's no netns support,
 * positive error code in case of an error. */
int netns_switch_root(void)
{
	int fd, res;

	fd = open("/proc/1/ns/net", O_RDONLY);
	if (fd < 0) {
		res = errno;
		if (res == ENOENT)
			res = -1;
		return res;
	}
	res = do_netns_switch(fd);
	close(fd);
	if (res == ENOENT)
		return -1;
	return res;
}

static void netns_list_destruct(struct netns_entry *entry)
{
	netns_handler_cleanup(entry);
	list_free(&entry->ids, NULL);
	if_list_free(&entry->ifaces);
	free(entry->name);
}

void netns_list_free(struct list *netns_list)
{
	list_free(netns_list, (destruct_f)netns_list_destruct);
}
