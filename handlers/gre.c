/*
 * This file is a part of plotnetcfg, a tool to visualize network config.
 * Copyright (C) 2017 Red Hat, Inc. -- Ondrej Hlavaty <ohlavaty@redhat.com>
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

#include "gre.h"
#include <arpa/inet.h>
#include <errno.h>
#include <linux/ip.h>
#include <linux/if.h>
#include <linux/if_tunnel.h>
#include "../addr.h"
#include "../handler.h"
#include "../if.h"
#include "../netlink.h"

static int gre_netlink(struct if_entry *entry, struct nlattr **linkinfo);

static struct if_handler h_gre = {
	.driver = "gre",
	.netlink = gre_netlink,
};

static struct if_handler h_gretap = {
	.driver = "gretap",
	.netlink = gre_netlink,
};

void handler_gre_register(void)
{
	if_handler_register(&h_gre);
	if_handler_register(&h_gretap);
}

static int gre_netlink(struct if_entry *entry, struct nlattr **linkinfo)
{
	struct nlattr **greinfo;
	int err, key;

	if (!linkinfo || !linkinfo[IFLA_INFO_DATA])
		return ENOENT;

	greinfo = nla_nested_attrs(linkinfo[IFLA_INFO_DATA], IFLA_GRE_MAX);
	if (!greinfo)
		return ENOMEM;

	if (greinfo[IFLA_GRE_LOCAL]) {
		struct addr addr;
		if ((err = addr_init(&addr, AF_INET, -1, nla_read(greinfo[IFLA_GRE_LOCAL]))))
			goto err_attrs;
		if (!addr_is_zero(&addr))
			if_add_config(entry, "local", "%s", addr.formatted);
		addr_destruct(&addr);
	}

	if (greinfo[IFLA_GRE_REMOTE]) {
		struct addr addr;
		if ((err = addr_init(&addr, AF_INET, -1, nla_read(greinfo[IFLA_GRE_REMOTE]))))
			goto err_attrs;
		if (!addr_is_zero(&addr))
			if_add_config(entry, "remote", "%s", addr.formatted);
		addr_destruct(&addr);
	}

	if (greinfo[IFLA_GRE_LINK])
		entry->link_index = nla_read_u8(greinfo[IFLA_GRE_LINK]);

	if (greinfo[IFLA_GRE_IKEY]) {
		if ((key = nla_read_u32(greinfo[IFLA_GRE_IKEY])))
			if_add_config(entry, "ikey", "%u", ntohl(key));
	}

	if (greinfo[IFLA_GRE_OKEY])
		if ((key = nla_read_u32(greinfo[IFLA_GRE_OKEY])))
			if_add_config(entry, "okey", "%u", ntohl(key));

	return 0;

err_attrs:
	free(greinfo);
	return err;
}
