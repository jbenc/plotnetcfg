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

#ifndef _UTILS_H
#define _UTILS_H

struct if_entry;

#define _unused __attribute__((unused))

typedef void (*destruct_f)(void *);
void list_free(void *list, destruct_f destruct);

/* Returns static buffer. */
char *ifstr(struct if_entry *entry);
char *ifid(struct if_entry *entry);

/* dst has to be allocated, at least 16 bytes. Returns the family or -1. */
int raw_addr(void *dst, const char *src);

#endif
