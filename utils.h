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
struct netns_entry;
struct rtable;

#define _unused __attribute__((unused))

#define OFFSETOF(s, i) ((size_t) &((s *)0)->i)
#define SKIP_BACK(s, i, p) ((s *)((char *)p - OFFSETOF(s, i)))
#define ARRAY_SIZE(a)	(sizeof(a) / sizeof(*a))


/* Returns static buffer. */
char *ifstr(struct if_entry *entry);
char *ifid(struct if_entry *entry);
char *nsid(struct netns_entry *entry);
char *rtid(struct rtable *rt);

#endif
