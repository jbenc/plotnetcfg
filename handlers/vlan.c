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

#include "vlan.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include "../handler.h"
#include "../if.h"
#include "../netlink.h"

struct vlan_private {
	unsigned int tag;
};

static int vlan_netlink(struct if_entry *entry, struct nlattr **linkinfo);

static struct if_handler h_vlan = {
	.driver = "802.1Q VLAN Support",
	.private_size = sizeof(struct vlan_private),
	.netlink = vlan_netlink,
};

void handler_vlan_register(void)
{
	if_handler_register(&h_vlan);
}

static int vlan_netlink(struct if_entry *entry, struct nlattr **linkinfo)
{
	struct vlan_private *priv = entry->handler_private;
	struct nlattr **vlanattr;
	int err = 0;

	if (!linkinfo || !linkinfo[IFLA_INFO_DATA])
		return ENOENT;
	vlanattr = nla_nested_attrs(linkinfo[IFLA_INFO_DATA], IFLA_VLAN_MAX);
	if (!vlanattr)
		return ENOMEM;
	if (!vlanattr[IFLA_VLAN_ID]) {
		err = ENOENT;
		goto out;
	}
	priv->tag = nla_read_u16(vlanattr[IFLA_VLAN_ID]);
	if (asprintf(&entry->edge_label, "tag %d", priv->tag) < 0) {
		err = ENOMEM;
		goto out;
	}
out:
	free(vlanattr);
	return err;
}
