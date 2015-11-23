/*
 * This file is a part of plotnetcfg, a tool to visualize network config.
 * Copyright (C) 2015 Red Hat, Inc. -- Ondrej Hlavaty <ohlavaty@redhat.com>
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include "sysfs.h"

#define PATH "/tmp/plotnetcfg-sys-XXXXXX"
#define LEN 26

char sysfs_mountpoint[LEN];
char *pathbuf = NULL;
long sysfs_path_max = 0;

void sysfs_clean()
{
	struct stat st;

	if (pathbuf)
		free(pathbuf);

	if (stat(sysfs_mountpoint, &st))
		return;

	if (S_ISDIR(st.st_mode))
		sysfs_umount();
}

int sysfs_init()
{
	strcpy(sysfs_mountpoint, PATH);
	if (sysfs_mountpoint != mkdtemp(sysfs_mountpoint))
		return errno;

	if ((sysfs_path_max = pathconf(sysfs_mountpoint, _PC_PATH_MAX)) < 0)
		return errno;
	sysfs_path_max += LEN + 1;

	if ((pathbuf = malloc(sysfs_path_max)) == NULL)
		return errno;
	strcpy(pathbuf, sysfs_mountpoint);
	pathbuf[LEN] = '/';

	atexit(sysfs_clean);
	return 0;
}

int sysfs_mount(const char *name)
{
	sysfs_umount();
	if (mount(name, sysfs_mountpoint, "sysfs", 0, NULL) < 0)
		return errno;
	return 0;
}

int sysfs_umount()
{
	if (umount2(sysfs_mountpoint, MNT_DETACH) < 0 && errno != EINVAL)
		return errno;
	return 0;
}

char *sysfs_realpath(const char *sys_path)
{
	char *resolved;
	strncpy(pathbuf + LEN + 1, sys_path, sysfs_path_max - LEN + 1);
	if ((resolved = realpath(pathbuf, NULL)) == NULL)
		return NULL;

	return resolved + LEN;
}

void sysfs_free(char *path)
{
	free(path - LEN);
}
