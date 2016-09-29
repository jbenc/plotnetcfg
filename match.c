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

#include "match.h"
#include <stdlib.h>
#include "if.h"
#include "master.h"
#include "netns.h"

int match_if_heur(struct if_entry **found,
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

int match_if(struct if_entry **found,
	     struct netns_entry *root, int all_ns,
	     struct if_entry *self,
	     int (*callback)(struct if_entry *, void *),
	     void *arg)
{
	struct netns_entry *ns;
	struct if_entry *entry;
	int res;

	for (ns = root; ns; ns = ns->next) {
		for (entry = ns->ifaces; entry; entry = entry->next) {
			if (entry == self)
				continue;
			res = callback(entry, arg);
			if (res < 0)
				return -res;
			if (res > 0) {
				*found = entry;
				return 0;
			}
		}
		if (!all_ns)
			break;
	}

	*found = NULL;
	return 0;
}

struct if_entry *match_if_netnsid(unsigned int ifindex, int netnsid,
				  struct netns_entry *current)
{
	struct netns_id *ptr;
	struct if_entry *entry;

	for (ptr = current->ids; ptr; ptr = ptr->next) {
		if (ptr->id == netnsid) {
			for (entry = ptr->ns->ifaces; entry; entry = entry->next) {
				if (entry->if_index == ifindex)
					return entry;
			}
			break;
		}
	}
	return NULL;
}

void match_all_netnsid(struct netns_entry *root)
{
	struct netns_entry *ns;
	struct if_entry *entry;

	for (ns = root; ns; ns = ns->next) {
		for (entry = ns->ifaces; entry; entry = entry->next) {
			if (entry->link_netnsid >= 0)
				link_set(match_if_netnsid(entry->link_index,
							  entry->link_netnsid,
							  ns), entry);
			if (entry->peer_netnsid >= 0)
				peer_set(entry, match_if_netnsid(entry->peer_index,
								 entry->peer_netnsid,
								 ns));
		}
	}
}
