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
#include "if.h"
#include "netns.h"

/* Find interface using a heuristic.
 * Callback returns 0 to ignore the interface, < 0 for error, > 0 for
 * priority.
 * The highest priority match is returned if exactly one highest priority
 * interface matches. Returns -1 if more highest priority interfaces match.
 * Returns 0 for success (*found will be NULL for no match) or error
 * code > 0.
 */
int match_if_heur(struct if_entry **found,
		  struct netns_entry *root, int all_ns,
		  struct if_entry *self,
		  int (*callback)(struct if_entry *, void *),
		  void *arg);

/* Find interface using netnsid. */
struct if_entry *match_if_netnsid(unsigned int ifindex, int netnsid,
				  struct netns_entry *current);

void match_all_netnsid(struct netns_entry *root);

#endif
