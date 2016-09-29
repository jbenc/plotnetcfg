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

#ifndef _SYSFS_H
#define _SYSFS_H

#include <sys/types.h>

int sysfs_init();
int sysfs_mount(const char *name);
void sysfs_umount();

/**
 * Calls readlink on sys_path inside sysfs.
 * The buffer it returns is allocated with malloc by realpath(3)
 * and must be freed by sysfs_free.
 */
char *sysfs_realpath(const char *sys_path);
void sysfs_free(char *ptr);

/*
 * Reads file into allocated buffer, must be freed after use.
 */
ssize_t sysfs_readfile(char **dest, const char *sys_path);

#endif
