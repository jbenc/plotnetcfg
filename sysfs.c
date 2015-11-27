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
#define LEN sizeof(PATH)

static char sysfs_mountpoint[LEN];

void sysfs_clean()
{
	struct stat st;

	if (stat(sysfs_mountpoint, &st))
		return;

	if (S_ISDIR(st.st_mode))
		sysfs_umount();

	rmdir(sysfs_mountpoint);
}

int sysfs_init()
{
	strcpy(sysfs_mountpoint, PATH);
	if (!mkdtemp(sysfs_mountpoint))
		return errno;

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

void sysfs_umount()
{
	umount2(sysfs_mountpoint, MNT_DETACH);
}

char *sysfs_realpath(const char *sys_path)
{
	char *resolved;
	char *path;

	if (asprintf(&path, "%s/%s", sysfs_mountpoint, sys_path) < 0)
		return NULL;

	resolved = realpath(path, NULL);

	free(path);

	return resolved ? resolved + LEN : NULL;
}

void sysfs_free(char *path)
{
	free(path - LEN);
}
