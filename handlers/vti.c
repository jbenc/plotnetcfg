/*
 * This file is a part of plotnetcfg, a tool to visualize network config.
 * Copyright (C) 2020 Red Hat, Inc. -- Sabrina Dubroca <sd@queasysnail.net>
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

#include "vti.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include "../addr.h"
#include "../handler.h"
#include "../if.h"
#include "../master.h"
#include "../netlink.h"
#include "../tunnel.h"
#include "../utils.h"

#include "../compat.h"

#include <linux/if.h>
#include <linux/ip.h>
#include <linux/if_tunnel.h>

static int vti4_netlink(struct if_entry *entry, struct nlattr **linkinfo);
static int vti6_netlink(struct if_entry *entry, struct nlattr **linkinfo);

static struct if_handler h_vti4 = {
	.driver = "vti",
	.netlink = vti4_netlink,
};

static struct if_handler h_vti6 = {
	.driver = "vti6",
	.netlink = vti6_netlink,
};

void handler_vti_register(void)
{
	if_handler_register(&h_vti4);
	if_handler_register(&h_vti6);
}

static int vti_netlink(int family, struct if_entry *entry, struct nlattr **linkinfo)
{
	struct nlattr **vtiinfo;
	int err, key;

	if (!linkinfo || !linkinfo[IFLA_INFO_DATA])
		return ENOENT;

	vtiinfo = nla_nested_attrs(linkinfo[IFLA_INFO_DATA], IFLA_VTI_MAX);
	if (!vtiinfo)
		return ENOMEM;

	if (vtiinfo[IFLA_VTI_REMOTE]) {
		struct addr addr;
		if ((err = addr_init(&addr, family, -1, nla_read(vtiinfo[IFLA_VTI_REMOTE]))))
			goto err_attrs;
		if (!addr_is_zero(&addr))
			if_add_config(entry, "remote", "%s", addr.formatted);
		addr_destruct(&addr);
	}

	if (vtiinfo[IFLA_VTI_LOCAL]) {
		struct addr addr;
		if ((err = addr_init(&addr, family, -1, nla_read(vtiinfo[IFLA_VTI_LOCAL]))))
			goto err_attrs;
		if (!addr_is_zero(&addr))
			if_add_config(entry, "local", "%s", addr.formatted);
		addr_destruct(&addr);
	}

	if (vtiinfo[IFLA_VTI_IKEY]) {
		if ((key = nla_read_be32(vtiinfo[IFLA_VTI_IKEY])))
			if_add_config(entry, "ikey", "%u", key);
	}

	if (vtiinfo[IFLA_VTI_OKEY]) {
		if ((key = nla_read_be32(vtiinfo[IFLA_VTI_OKEY])))
			if_add_config(entry, "okey", "%u", key);
	}

err_attrs:
	free(vtiinfo);
	return err;
}

static int vti4_netlink(struct if_entry *entry, struct nlattr **linkinfo)
{
	return vti_netlink(AF_INET, entry, linkinfo);
}

static int vti6_netlink(struct if_entry *entry, struct nlattr **linkinfo)
{
	return vti_netlink(AF_INET6, entry, linkinfo);
}
