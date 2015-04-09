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

#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <libnetlink.h>
#include "handler.h"
#include "if.h"
#include "match.h"
#include "utils.h"
#include "netns.h"

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

static struct netns_entry *netns_check_duplicate(struct netns_entry *root,
						 long int kernel_id)
{
	while (root) {
		if (root->kernel_id == kernel_id)
			return root;
		root = root->next;
	}
	return NULL;
}

static int netns_get_var_entry(struct netns_entry **result,
			       struct netns_entry *root,
			       const char *name)
{
	struct netns_entry *entry;
	char path[PATH_MAX];
	long kernel_id;
	int err;

	*result = entry = calloc(sizeof(struct netns_entry), 1);
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
	if (netns_check_duplicate(root, kernel_id)) {
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
				struct netns_entry *root,
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
	dup = netns_check_duplicate(root, kernel_id);
	if (dup) {
		if (dup->pid && (dup->pid > pid)) {
			dup->pid = pid;
			netns_proc_entry_set_name(dup, spid);
		}
		return -1;
	}

	*result = entry = calloc(sizeof(struct netns_entry), 1);
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

static int netns_new_list(struct netns_entry **root, int supported)
{
	*root = calloc(sizeof(struct netns_entry), 1);
	if (!*root)
		return ENOMEM;
	if (supported) {
		(*root)->kernel_id = netns_get_kernel_id("/proc/1/ns/net");
		if ((*root)->kernel_id < 0)
			return -(*root)->kernel_id;
		(*root)->fd = open("/proc/1/ns/net", O_RDONLY);
		if ((*root)->fd < 0)
			return errno;
	}
	return 0;
}

static int netns_add_var_list(struct netns_entry *root)
{
	struct netns_entry *entry, *ptr;
	struct dirent *de;
	DIR *dir;
	int err;

	dir = opendir(NETNS_RUN_DIR);
	if (!dir)
		return 0;

	ptr = root;
	while (ptr->next)
		ptr = ptr->next;

	while ((de = readdir(dir)) != NULL) {
		if (!strcmp(de->d_name, ".") ||
		    !strcmp(de->d_name, ".."))
			continue;
		err = netns_get_var_entry(&entry, root, de->d_name);
		if (err < 0) {
			/* duplicate entry */
			continue;
		}
		if (err)
			return err;
		ptr->next = entry;
		ptr = entry;
	}
	closedir(dir);

	return 0;
}

static int netns_add_proc_list(struct netns_entry *root)
{
	struct netns_entry *entry, *ptr;
	struct dirent *de;
	DIR *dir;
	int err;

	dir = opendir("/proc");
	if (!dir)
		return 0;

	ptr = root;
	while (ptr->next)
		ptr = ptr->next;

	while ((de = readdir(dir)) != NULL) {
		if (!strcmp(de->d_name, ".") ||
		    !strcmp(de->d_name, ".."))
			continue;
		if (de->d_name[0] < '0' || de->d_name[1] > '9')
			continue;
		err = netns_get_proc_entry(&entry, root, de->d_name);
		if (err < 0) {
			/* duplicate entry */
			continue;
		}
		if (err)
			return err;
		ptr->next = entry;
		ptr = entry;
	}
	closedir(dir);

	return 0;
}

/* Returns -1 if netnsids are not supported. */
static int netns_get_id(struct rtnl_handle *rth, struct netns_entry *entry)
{
	struct {
		struct nlmsghdr nlh;
		struct rtgenmsg rtm;
		char buf[1024];
	} msg;
	struct rtattr *tb[NETNSA_MAX + 1];
	int len;

	memset(&msg, 0, sizeof(msg));
	msg.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtgenmsg));
	msg.nlh.nlmsg_flags = NLM_F_REQUEST;
	msg.nlh.nlmsg_type = RTM_GETNSID;
	msg.rtm.rtgen_family = AF_UNSPEC;
	addattr32(&msg.nlh, 1024, NETNSA_FD, entry->fd);
	if (rtnl_talk(rth, &msg.nlh, 0, 0, &msg.nlh) < 0)
		return -1;
	if (msg.nlh.nlmsg_type == NLMSG_ERROR)
		return -1;
	len = msg.nlh.nlmsg_len - NLMSG_SPACE(sizeof(struct rtgenmsg));
	if (len < 0)
		return -1;
	parse_rtattr(tb, NETNSA_MAX, NETNS_RTA(&msg.rtm), len);
	if (tb[NETNSA_NSID])
		return *(__s32 *)RTA_DATA(tb[NETNSA_NSID]);
	return -1;
}

/* This is best effort only, if anything fails (e.g. netnsids are not
 * supported by kernel), we fail back to heuristics. */
static void netns_get_all_ids(struct netns_entry *current, struct netns_entry *root)
{
	struct rtnl_handle rth;
	struct netns_entry *entry;
	struct netns_id *nsid, *ptr = NULL;
	int id;

	if (netns_switch(current))
		return;
	if (rtnl_open(&rth, 0) < 0)
		return;
	for (entry = root; entry; entry = entry->next) {
		id = netns_get_id(&rth, entry);
		if (id < 0)
			continue;
		nsid = malloc(sizeof(*nsid));
		if (!nsid)
			break;
		nsid->next = NULL;
		nsid->ns = entry;
		nsid->id = id;
		if (!ptr)
			current->ids = nsid;
		else
			ptr->next = nsid;
		ptr = nsid;
	}
	rtnl_close(&rth);
}

int netns_list(struct netns_entry **result, int supported)
{
	struct netns_entry *entry;
	int err;

	err = netns_new_list(result, supported);
	if (err)
		return err;
	if (supported) {
		err = netns_add_var_list(*result);
		if (err)
			return err;
		err = netns_add_proc_list(*result);
		if (err)
			return err;
	}
	for (entry = *result; entry; entry = entry->next) {
		if (entry->name) {
			/* Do not try to switch to the root netns, as we're
			 * already there when processing the first entry,
			 * and netns_switch fails hard if there's no netns
			 * support available. */
			if ((err = netns_switch(entry)))
				return err;
		}
		if ((err = if_list(&entry->ifaces, entry)))
			return err;
	}
	/* Walk all net name spaces again and gather all kernel assigned
	 * netnsids. We don't assign netnsids ourselves to prevent assigning
	 * them needlessly - the kernel assigns only those that are really
	 * needed while doing netlinks dumps. Note also that netnsids are
	 * per name space and this is O(n^2). */
	for (entry = *result; entry; entry = entry->next)
		netns_get_all_ids(entry, *result);
	/* And finally, resolve netnsid+ifindex to the if_entry pointers. */
	match_all_netnsid(*result);

	if ((err = global_handler_post(*result)))
		return err;
	if ((err = handler_post(*result)))
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
	if (fd < 0)
		return errno;
	res = do_netns_switch(fd);
	close(fd);
	if (res == ENOENT)
		return -1;
	return res;
}

static void netns_list_destruct(struct netns_entry *entry)
{
	list_free(entry->ids, NULL);
	if_list_free(entry->ifaces);
	free(entry->name);
}

void netns_list_free(struct netns_entry *list)
{
	list_free(list, (destruct_f)netns_list_destruct);
}
