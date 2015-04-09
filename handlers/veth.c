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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../ethtool.h"
#include "../handler.h"
#include "../match.h"
#include "../utils.h"
#include "veth.h"

static int veth_scan(struct if_entry *entry);
static int veth_post(struct if_entry *entry, struct netns_entry *root);

static struct handler h_veth = {
	.driver = "veth",
	.scan = veth_scan,
	.post = veth_post,
};

void handler_veth_register(void)
{
	handler_register(&h_veth);
}

static int veth_scan(struct if_entry *entry)
{
	entry->peer_index = ethtool_veth_peer(entry->if_name);
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

static int veth_post(struct if_entry *entry, struct netns_entry *root)
{
	int err;

	if (!entry->peer_index)
		return ENOENT;
	err = find_interface(&entry->peer, root, 1, entry, match_peer, entry);
	if (err > 0)
		return err;
	if (err < 0)
		return if_add_warning(entry, "failed to find the veth peer reliably");
	if (!entry->peer)
		return if_add_warning(entry, "failed to find the veth perr");
	return 0;
}
