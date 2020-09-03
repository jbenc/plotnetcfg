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

#include "xfrm.h"
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

static int xfrm_netlink(struct if_entry *entry, struct nlattr **linkinfo);

static struct if_handler h_xfrm = {
	.driver = "xfrm",
	.netlink = xfrm_netlink,
};

void handler_xfrm_register(void)
{
	if_handler_register(&h_xfrm);
}

static int xfrm_netlink(struct if_entry *entry, struct nlattr **linkinfo)
{
	struct nlattr **xfrminfo;
	int if_id;

	if (!linkinfo || !linkinfo[IFLA_INFO_DATA])
		return ENOENT;

	xfrminfo = nla_nested_attrs(linkinfo[IFLA_INFO_DATA], IFLA_XFRM_MAX);
	if (!xfrminfo)
		return ENOMEM;

	if (xfrminfo[IFLA_XFRM_IF_ID]) {
		if_id = nla_read_u32(xfrminfo[IFLA_XFRM_IF_ID]);
		if_add_config(entry, "if_id", "0x%x", if_id);
	}

	free(xfrminfo);
	return 0;
}
