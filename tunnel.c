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

#include <arpa/inet.h>
#include <string.h>
#include "handler.h"
#include "addr.h"
#include "if.h"
#include "match.h"
#include "netns.h"
#include "utils.h"
#include "tunnel.h"

static int match_tunnel(struct if_entry *entry, void *arg)
{
	struct addr *data = arg;
	struct if_addr_entry *addr;

	if (!(entry->flags & IF_UP))
		return 0;
	for (addr = entry->addr; addr; addr = addr->next) {
		if (addr->addr.family != data->family)
			continue;
		if (!memcmp(addr->addr.raw, data->raw, data->family == AF_INET ? 4 : 16))
			return 1;
	}
	return 0;
}

struct if_entry *tunnel_find_str(struct netns_entry *ns, const char *addr)
{
	struct addr data;
	char buf [16];
	struct if_entry *result;

	data.raw = buf;
	data.family = addr_parse_raw(data.raw, addr);
	if (data.family < 0)
		return NULL;
	if (match_if_heur(&result, ns, 0, NULL, match_tunnel, &data))
		return NULL;
	return result;
}

struct if_entry *tunnel_find_addr(struct netns_entry *ns, struct addr *addr)
{
	struct if_entry *result;

	if (match_if_heur(&result, ns, 0, NULL, match_tunnel, addr))
		return NULL;
	return result;
}
