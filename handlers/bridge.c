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

#include "bridge.h"
#include <errno.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include "../handler.h"
#include "../if.h"
#include "../sysfs.h"

static int bridge_scan(struct if_entry *entry);

static struct if_handler h_bridge = {
	.scan = bridge_scan,
};

void handler_bridge_register(void)
{
	if_handler_register(&h_bridge);
}

static int bridge_scan(struct if_entry *entry)
{
	int len;
	char path [33 + IFNAMSIZ + 1];
	char *ifindex;

	if (entry->master_index)
		return 0;

	snprintf(path, sizeof(path), "/class/net/%s/brport/bridge/ifindex", entry->if_name);

	len = sysfs_readfile(&ifindex, path);
	if (len < 0) {
		if (len == -ENOENT)
			return 0;
		return -len;
	}

	entry->master_index = atoi(ifindex);
	free(ifindex);
	return 0;
}
