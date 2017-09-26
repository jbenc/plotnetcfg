/*
 * This file is a part of plotnetcfg, a tool to visualize network config.
 * Copyright (C) 2016 Red Hat, Inc. -- Jiri Benc <jbenc@redhat.com>,
 *                                     Ondrej Hlavaty <ohlavaty@redhat.com>
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

/* Resolves master_index to the actual interface. */

#include "if.h"
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include "master.h"
#include "match.h"
#include "netns.h"

int master_set(struct if_entry *master, struct if_entry *master_of)
{
	if (master_of->master != NULL)
		node_remove(&master_of->rev_master_node);
	master_of->master = master;
	if (master_of->master != NULL)
		list_append(&master_of->master->rev_master, &master_of->rev_master_node);

	return 0;
}

int link_set(struct if_entry *link, struct if_entry *entry)
{
	if (!entry)
		return 0;

	if (entry->link != NULL)
		node_remove(&entry->rev_link_node);
	entry->link = link;
	if (entry->link != NULL)
		list_append(&entry->link->rev_link, &entry->rev_link_node);

	return 0;
}

int peer_set(struct if_entry *first, struct if_entry *second)
{
	if (first) {
		if (first->peer)
			first->peer->peer = NULL;
		first->peer = second;
	}

	if (second) {
		if (second->peer)
			second->peer->peer = NULL;
		second->peer = first;
	}
	return 0;
}

static int match_master(struct if_entry *entry, void *arg)
{
	struct if_entry *slave = arg;

	if (entry->if_index != slave->master_index)
		return 0;
	if (entry->ns == slave->ns)
		return 2;
	return 1;
}

static int match_link(struct if_entry *entry, void *arg)
{
	struct if_entry *slave = arg;

	if (entry->if_index != slave->link_index)
		return 0;
	if (entry->ns == slave->ns)
		return 2;
	return 1;
}

static int err_msg(struct match_desc *match, const char *type, struct if_entry *entry)
{
	if (match_ambiguous(*match))
		return if_add_warning(entry, "has a %s but failed to find it reliably", type);
	if (!match_found(*match))
		return if_add_warning(entry, "has a %s but failed to find it", type);
	return 0;
}

static int process(struct if_entry *entry, struct list *netns_list)
{
	int err;
	struct match_desc match;

	if (!entry->master && entry->master_index) {
		match_init(&match);
		match.netns_list = netns_list;
		match.exclude = entry;

		if ((err = match_if(&match, match_master, entry)))
			return err;
		if ((err = err_msg(&match, "master", entry)))
			return err;
		if ((err = master_set(match_found(match), entry)))
			return err;
	}
	if (!entry->link && entry->link_index) {
		match_init(&match);
		match.netns_list = netns_list;
		match.exclude = entry;

		if ((err = match_if(&match, match_link, entry)))
			return err;
		if ((err = err_msg(&match, "link", entry)))
			return err;
		if ((err = link_set(match_found(match), entry)))
			return err;
	}
	return 0;
}

int master_resolve(struct list *netns_list)
{
	struct netns_entry *ns;
	struct if_entry *entry;
	int err;

	list_for_each(ns, *netns_list) {
		list_for_each(entry, ns->ifaces) {
			err = process(entry, netns_list);
			if (err)
				return err;
		}
	}
	return 0;
}
