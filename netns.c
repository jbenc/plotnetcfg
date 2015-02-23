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
#include "handler.h"
#include "if.h"
#include "utils.h"
#include "netns.h"

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

static int netns_check_duplicate(struct netns_entry *root, long int kernel_id)
{
	while (root) {
		if (root->kernel_id == kernel_id)
			return 1;
		root = root->next;
	}
	return 0;
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

static int netns_get_proc_entry(struct netns_entry **result,
				struct netns_entry *root,
				const char *pid)
{
	struct netns_entry *entry;
	char path[PATH_MAX], buf[PATH_MAX];
	long kernel_id;
	ssize_t len;
	int commfd;

	snprintf(path, sizeof(path), "/proc/%s/ns/net", pid);
	kernel_id = netns_get_kernel_id(path);
	if (kernel_id < 0) {
		/* ignore entries that cannot be read */
		return -1;
	}
	if (netns_check_duplicate(root, kernel_id))
		return -1;

	*result = entry = calloc(sizeof(struct netns_entry), 1);
	if (!entry)
		return ENOMEM;

	entry->kernel_id = kernel_id;
	entry->fd = open(path, O_RDONLY);
	if (entry->fd < 0) {
		/* ignore entries that cannot be read */
		free(entry);
		return -1;
	}

	snprintf(path, sizeof(path), "/proc/%s/comm", pid);
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
		snprintf(buf, sizeof(buf), "PID %s (%s)", pid, path);
	else
		snprintf(buf, sizeof(buf), "PID %s", pid);
	entry->name = strdup(buf);
	if (!entry->name)
		return ENOMEM;
	return 0;
}

static int netns_new_list(struct netns_entry **root)
{
	long kernel_id;

	*root = calloc(sizeof(struct netns_entry), 1);
	if (!*root)
		return ENOMEM;
	kernel_id = netns_get_kernel_id("/proc/1/ns/net");
	if (kernel_id < 0)
		kernel_id = 0;
	(*root)->kernel_id = kernel_id;
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

int netns_list(struct netns_entry **result, int supported)
{
	struct netns_entry *entry;
	int err;

	err = netns_new_list(result);
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
		if (entry->name)
			if ((err = netns_switch(entry)))
				return err;
		if ((err = if_list(&entry->ifaces, entry)))
			return err;
	}
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
	if_list_free(entry->ifaces);
	free(entry->name);
}

void netns_list_free(struct netns_entry *list)
{
	list_free(list, (destruct_f)netns_list_destruct);
}
