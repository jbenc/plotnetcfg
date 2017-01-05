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

#include "veth.h"
#include <errno.h>
#include <string.h>
#include "../ethtool.h"
#include "../handler.h"
#include "../if.h"
#include "../master.h"
#include "../match.h"

struct netns_entry;

static int veth_scan(struct if_entry *entry);
static int veth_post(struct if_entry *entry, struct list *netns_list);

static struct if_handler h_veth = {
	.driver = "veth",
	.scan = veth_scan,
	.post = veth_post,
};

void handler_veth_register(void)
{
	if_handler_register(&h_veth);
}

static int veth_scan(struct if_entry *entry)
{
	if (entry->link_index) {
		entry->peer_index = entry->link_index;
		entry->peer_netnsid = entry->link_netnsid;
		entry->link_index = 0;
		entry->link_netnsid = -1;
	} else {
		entry->peer_index = ethtool_veth_peer(entry->if_name);
	}
	return 0;
}

static int match_peer(struct if_entry *entry, void *arg)
{
	struct if_entry *link = arg;

	if (entry->if_index != link->peer_index ||
	    entry->peer_index != link->if_index ||
	    strcmp(entry->driver, "veth"))
		return 0;
	if (entry->peer && entry->peer != link)
		return 0;
	if (entry->ns == link->ns)
		return 2;
	return 1;
}

static int veth_post(struct if_entry *entry, struct list *netns_list)
{
	int err;
	struct match_desc match;

	if (entry->peer)
		return 0;
	if (!entry->peer_index)
		return ENOENT;

	match_init(&match);
	match.netns_list = netns_list;
	match.exclude = entry;

	if ((err = match_if(&match, match_peer, entry)))
		return err;
	if (match_ambiguous(match))
		return if_add_warning(entry, "failed to find the veth peer reliably");
	if (!match_found(match))
		return if_add_warning(entry, "failed to find the veth perr");
	peer_set(entry, match_found(match));
	return 0;
}
