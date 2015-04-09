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

/* Resolves master_index to the actual interface. */

#include <errno.h>
#include <stdio.h>
#include "../handler.h"
#include "../match.h"
#include "../utils.h"
#include "master.h"

static int master_post(struct netns_entry *root);

static struct global_handler h_master = {
	.post = master_post,
};

void handler_master_register(void)
{
	global_handler_register(&h_master);
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

	if (entry->master_index) {
		err = match_if_heur(&entry->master, root, 1, entry, match_master, entry);
		if ((err = err_msg(err, "master", entry, entry->master)))
			return err;
	}
	if (!entry->link && entry->link_index) {
		err = match_if_heur(&entry->link, root, 1, entry, match_link, entry);
		if ((err = err_msg(err, "link", entry, entry->link)))
			return err;
	}
	return 0;
}

static int master_post(struct netns_entry *root)
{
	struct netns_entry *ns;
	struct if_entry *entry;
	int err;

	for (ns = root; ns; ns = ns->next) {
		for (entry = ns->ifaces; entry; entry = entry->next) {
			err = process(entry, root);
			if (err)
				return err;
		}
	}
	return 0;
}
