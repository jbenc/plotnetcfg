/*
 * This file is a part of plotnetcfg, a tool to visualize network config.
 * Copyright (C) 2016 Red Hat, Inc. -- Ondrej Hlavaty <ohlavaty@redhat.com>
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

#include "vxlan.h"
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

#define VXLAN_DEFAULT_PORT 46354

struct vxlan_priv {
	struct addr *local;
	struct addr *group;
	int flags;
};

static int vxlan_netlink(struct if_entry *entry, struct rtattr **linkinfo);
static int vxlan_post(struct if_entry *entry, struct netns_entry *root);

static struct handler h_vxlan = {
	.driver = "vxlan",
	.private_size = sizeof(struct vxlan_priv),
	.netlink = vxlan_netlink,
	.post = vxlan_post,
};

#define VXLAN_COLLECT_METADATA 1

void handler_vxlan_register(void)
{
	handler_register(&h_vxlan);
}

static int vxlan_fill_addr(struct addr **addr, int ai_family, struct rtattr *rtattr)
{
	int err;

	if (!rtattr || *addr)
		return 0;

	*addr = calloc(1, sizeof(struct addr));
	if (!*addr)
		return ENOMEM;

	if ((err = addr_init(*addr, ai_family, ai_family == AF_INET ? 32 : 128, RTA_DATA(rtattr)))) {
		free(*addr);
		*addr = NULL;
		return err;
	}

	return 0;
}

static int vxlan_netlink(struct if_entry *entry, struct rtattr **linkinfo)
{
	struct rtattr *vxlaninfo[IFLA_VXLAN_MAX + 1];
	uint16_t port;
	struct vxlan_priv *priv;
	int err;

	priv = calloc(1, sizeof(struct vxlan_priv));
	if (!priv) {
		err = ENOMEM;
		goto err;
	}
	entry->handler_private = priv;

	if (!linkinfo || !linkinfo[IFLA_INFO_DATA]) {
		err = ENOENT;
		goto err_priv;
	}
	rtnl_parse_nested(vxlaninfo, IFLA_VXLAN_MAX, linkinfo[IFLA_INFO_DATA]);

	if (vxlaninfo[IFLA_VXLAN_ID])
		if_add_config(entry, "VNI", "%u", *(uint32_t *)RTA_DATA(vxlaninfo[IFLA_VXLAN_ID]));

	if (vxlaninfo[IFLA_VXLAN_PORT]) {
		port = *(uint16_t *)RTA_DATA(vxlaninfo[IFLA_VXLAN_PORT]);
		if (port != VXLAN_DEFAULT_PORT)
			if_add_config(entry, "port", "%u", port);
	}

	if (vxlaninfo[IFLA_VXLAN_COLLECT_METADATA]) {
		priv->flags |= (*(uint8_t *) RTA_DATA(vxlaninfo[IFLA_VXLAN_COLLECT_METADATA])) ? VXLAN_COLLECT_METADATA : 0;
	}

	if (priv->flags & VXLAN_COLLECT_METADATA) {
		if_add_config(entry, "mode", "external");
	} else {
		/* These can be set in COLLECT_METADATA, but are ignored by kernel */
		if ((err = vxlan_fill_addr(&priv->group, AF_INET, vxlaninfo[IFLA_VXLAN_GROUP])))
			goto err_priv;
		if ((err = vxlan_fill_addr(&priv->group, AF_INET6, vxlaninfo[IFLA_VXLAN_GROUP6])))
			goto err_priv;
		if ((err = vxlan_fill_addr(&priv->local, AF_INET, vxlaninfo[IFLA_VXLAN_LOCAL])))
			goto err_priv;
		if ((err = vxlan_fill_addr(&priv->local, AF_INET6, vxlaninfo[IFLA_VXLAN_LOCAL6])))
			goto err_priv;
	}

	return 0;

err_priv:
	free(priv);
err:
	return err;
}

static int vxlan_post(struct if_entry *entry, _unused struct netns_entry *root)
{
	struct vxlan_priv *priv;
	struct if_entry *ife;

	priv = (struct vxlan_priv *) entry->handler_private;
	if (priv->local) {
		if_add_config(entry, "from", "%s", priv->local->formatted);
		if ((ife = tunnel_find_addr(entry->ns, priv->local))) {
			link_set(ife, entry);
			entry->flags |= IF_LINK_WEAK;
		}
	}
	if (priv->group)
		if_add_config(entry, "to", "%s", priv->group->formatted);
	return 0;
}
