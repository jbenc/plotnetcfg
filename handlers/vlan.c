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

#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "../handler.h"
#include "../netlink.h"
#include "vlan.h"

static int vlan_netlink(struct if_entry *entry, struct rtattr **tb);

static struct handler h_vlan = {
	.driver = "802.1Q VLAN Support",
	.netlink = vlan_netlink,
	.cleanup = handler_generic_cleanup,
};

struct vlan_private {
	unsigned int tag;
};

void handler_vlan_register(void)
{
	handler_register(&h_vlan);
}

static int vlan_netlink(struct if_entry *entry, struct rtattr **tb)
{
	struct vlan_private *priv;
	struct rtattr *linkinfo[IFLA_INFO_MAX + 1];
	struct rtattr *vlanattr[IFLA_VLAN_MAX + 1];

	priv = calloc(sizeof(*priv), 1);
	if (!priv)
		return ENOMEM;
	entry->handler_private = priv;

	if (!tb[IFLA_LINKINFO])
		return ENOENT;
	rtnl_parse_nested(linkinfo, IFLA_INFO_MAX, tb[IFLA_LINKINFO]);
	if (!linkinfo[IFLA_INFO_DATA])
		return ENOENT;
	rtnl_parse_nested(vlanattr, IFLA_VLAN_MAX, linkinfo[IFLA_INFO_DATA]);
	if (!vlanattr[IFLA_VLAN_ID] ||
	    RTA_PAYLOAD(vlanattr[IFLA_VLAN_ID]) < sizeof(__u16))
		return ENOENT;
	priv->tag = *(uint16_t *)RTA_DATA(vlanattr[IFLA_VLAN_ID]);
	if (asprintf(&entry->edge_label, "tag %d", priv->tag) < 0)
		return ENOMEM;
	return 0;
}
