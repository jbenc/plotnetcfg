/*
 * This file is a part of plotnetcfg, a tool to visualize network config.
 * Copyright (C) 2014-2015 Red Hat, Inc. -- Jiri Benc <jbenc@redhat.com>
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

#ifndef _MATCH_H
#define _MATCH_H

#include <string.h>
#include "list.h"

struct if_entry;
struct netns_entry;

typedef int (*match_callback_f)(struct if_entry *, void *);

#define MM_HEURISTIC	0
#define MM_FIRST	1

struct match_desc {
	/* Mode of searching, heuristic by default */
	int mode; /* MM_* */

	/* Exclude one entry from result (e.g. self when finding peer) */
	struct if_entry *exclude;

	/* Search in netns list or one ns */
	struct list *netns_list;
	struct netns_entry *ns;

	/* Internal. Use match_found and match_ambiguous functions. */
	struct if_entry *found;
	int best, count;
};

/* Find interface with various criteria. See match_desc for description.
 *
 * Compare callback:
 * Returns > 0 as a priority of the match,
 *           0 to ignore (not matched),
 *         < 0 for error.
 *
 * The highest priority match is returned if exactly one highest priority
 * interface matches.
 * Returns 0 for success or error code > 0.
 * Use match_found and match_is_ambiguous macros to retrieve result.
 */
int match_if(struct match_desc *desc, match_callback_f callback, void *arg);

#define match_found(d)		((d).best > 0 ? (d).found : NULL)
#define match_ambiguous(d)	((d).best > 0 && (d).count > 1)

static inline void match_init(struct match_desc *desc)
{
	memset(desc, 0, sizeof(struct match_desc));
}

/* Find interface using netnsid. */
struct if_entry *match_if_netnsid(unsigned int ifindex, int netnsid,
				  struct netns_entry *current);

void match_all_netnsid(struct list *netns_list);

#endif
