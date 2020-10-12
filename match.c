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

/* Matches only on desc->ns */
static int match_if_ns(struct match_desc *desc, match_callback_f callback, void *arg)
{
	struct if_entry *entry;
	int res;

	list_for_each(entry, desc->ns->ifaces) {
		if (entry == desc->exclude)
			continue;
		res = callback(entry, arg);
		if (res < 0)
			return -res;
		if (res > desc->best) {
			desc->found = entry;
			desc->best = res;
			desc->count = 1;
			if (desc->mode == MM_FIRST)
				return 0;
		} else if (res == desc->best)
			desc->count++;
	}

	return 0;
}

int match_if(struct match_desc *desc, match_callback_f callback, void *arg)
{
	struct netns_entry *ns;
	int err;

	if (desc->netns_list) {
		list_for_each(ns, *desc->netns_list) {
			desc->ns = ns;
			if ((err = match_if_ns(desc, callback, arg)))
				return err;
		}
		desc->ns = NULL;
		return 0;
	}

	return match_if_ns(desc, callback, arg);
}

struct netns_entry *match_netnsid(int netnsid, struct netns_entry *current)
{
	struct netns_id *ptr;

	list_for_each(ptr, current->ids) {
		if (ptr->id == netnsid)
			return ptr->ns;
	}
	return NULL;
}

struct if_entry *match_if_netnsid(unsigned int ifindex, int netnsid,
				  struct netns_entry *current)
{
	struct netns_entry *ptr = match_netnsid(netnsid, current);
	struct if_entry *entry;

	if (!ptr)
		return NULL;

	list_for_each(entry, ptr->ifaces) {
		if (entry->if_index == ifindex)
			return entry;
	}

	return NULL;
}

void match_all_netnsid(struct list *netns_list)
{
	struct netns_entry *ns;
	struct if_entry *entry;

	list_for_each(ns, *netns_list) {
		list_for_each(entry, ns->ifaces) {
			if (entry->link_netnsid >= 0) {
				if (entry->link_index)
					link_set(match_if_netnsid(entry->link_index,
								  entry->link_netnsid,
								  ns), entry);
				else
					entry->link_net = match_netnsid(entry->link_netnsid, ns);
			}
			if (entry->peer_netnsid >= 0)
				peer_set(entry, match_if_netnsid(entry->peer_index,
								 entry->peer_netnsid,
								 ns));
		}
	}
}
