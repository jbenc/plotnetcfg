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
#include "if.h"
#include "netns.h"
#include "utils.h"
#include "tunnel.h"

struct search_arg {
	int af;
	char raw[16];
};

static int match_tunnel(struct if_entry *entry, void *arg)
{
	struct search_arg *data = arg;
	struct if_addr_entry *addr;

	if (!(entry->flags & IF_UP))
		return 0;
	for (addr = entry->addr; addr; addr = addr->next) {
		if (addr->addr.family != data->af)
			continue;
		if (!memcmp(addr->addr.raw, data->raw, data->af == AF_INET ? 4 : 16))
			return 1;
	}
	return 0;
}

struct if_entry *tunnel_find_iface(struct netns_entry *ns, const char *addr)
{
	struct search_arg data;
	struct if_entry *result;

	data.af = raw_addr(data.raw, addr);
	if (data.af < 0)
		return NULL;
	if (find_interface(&result, ns, 0, NULL, match_tunnel, &data))
		return NULL;
	return result;
}
