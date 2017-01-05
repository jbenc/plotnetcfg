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
	if (entry->link != NULL)
		node_remove(&entry->rev_link_node);
	entry->link = link;
	if (entry->link != NULL)
		list_append(&entry->link->rev_link, &entry->rev_link_node);

	return 0;
}

int peer_set(struct if_entry *first, struct if_entry *second)
{
	if (first->peer)
		first->peer->peer = NULL;
	if (second->peer)
		second->peer->peer = NULL;

	first->peer = second;
	second->peer = first;
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

static int err_msg(int err, const char *type, struct if_entry *entry,
		   struct if_entry *check)
{
	if (err > 0)
		return err;
	if (err < 0)
		return if_add_warning(entry, "has a %s but failed to find it reliably", type);
	if (!check)
		return if_add_warning(entry, "has a %s but failed to find it", type);
	return 0;
}

static int process(struct if_entry *entry, struct netns_entry *root)
{
	int err;
	struct if_entry *master, *link;

	if (!entry->master && entry->master_index) {
		err = match_if_heur(&master, root, 1, entry, match_master, entry);
		if ((err = err_msg(err, "master", entry, master)))
			return err;
		if ((err = master_set(master, entry)))
			return err;
	}
	if (!entry->link && entry->link_index) {
		err = match_if_heur(&link, root, 1, entry, match_link, entry);
		if ((err = err_msg(err, "link", entry, link)))
			return err;
		if ((err = link_set(link, entry)))
			return err;
	}
	return 0;
}

int master_resolve(struct netns_entry *root)
{
	struct netns_entry *ns;
	struct if_entry *entry;
	int err;

	for (ns = root; ns; ns = ns->next) {
		list_for_each(entry, ns->ifaces) {
			err = process(entry, root);
			if (err)
				return err;
		}
	}
	return 0;
}
