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

#include <stdlib.h>
#include "netns.h"
#include "if.h"
#include "match.h"

int find_interface(struct if_entry **found,
		   struct netns_entry *root, int all_ns,
		   struct if_entry *self,
		   int (*callback)(struct if_entry *, void *),
		   void *arg)
{
	struct netns_entry *ns;
	struct if_entry *entry;
	int prio = 0, count = 0, res;

	for (ns = root; ns; ns = ns->next) {
		for (entry = ns->ifaces; entry; entry = entry->next) {
			if (entry == self)
				continue;
			res = callback(entry, arg);
			if (res < 0)
				return -res;
			if (res > prio) {
				*found = entry;
				prio = res;
				count = 1;
			} else if (res == prio)
				count++;
		}
		if (!all_ns)
			break;
	}
	if (!prio) {
		*found = NULL;
		return 0;
	}
	if (count > 1)
		return -1;
	return 0;
}
