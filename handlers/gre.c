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
#include "../master.h"
#include "../netlink.h"
#include "../tunnel.h"
#include "../netns.h"

static int gre_netlink(struct if_entry *entry, struct nlattr **linkinfo);
static int gre6_netlink(struct if_entry *entry, struct nlattr **linkinfo);
static int gre_post(struct if_entry *entry, struct list *netns_list);

struct gre_priv {
	struct addr local;
};

static struct if_handler h_gre = {
	.driver = "gre",
	.private_size = sizeof(struct gre_priv),
	.netlink = gre_netlink,
	.post = gre_post,
};

static struct if_handler h_gretap = {
	.driver = "gretap",
	.private_size = sizeof(struct gre_priv),
	.netlink = gre_netlink,
	.post = gre_post,
};

static struct if_handler h_ip6gre = {
	.driver = "ip6gre",
	.private_size = sizeof(struct gre_priv),
	.netlink = gre6_netlink,
	.post = gre_post,
};

static struct if_handler h_ip6gretap = {
	.driver = "ip6gretap",
	.private_size = sizeof(struct gre_priv),
	.netlink = gre6_netlink,
	.post = gre_post,
};

void handler_gre_register(void)
{
	if_handler_register(&h_gre);
	if_handler_register(&h_gretap);
	if_handler_register(&h_ip6gre);
	if_handler_register(&h_ip6gretap);
}

static int gre_common_netlink(int family, struct if_entry *entry, struct nlattr **linkinfo)
{
	struct nlattr **greinfo;
	struct gre_priv *priv;
	int err, key;

	if (!linkinfo || !linkinfo[IFLA_INFO_DATA])
		return ENOENT;

	priv = calloc(1, sizeof(struct gre_priv));
	if (!priv)
		return ENOMEM;
	entry->handler_private = priv;

	greinfo = nla_nested_attrs(linkinfo[IFLA_INFO_DATA], IFLA_GRE_MAX);
	if (!greinfo) {
		err = ENOMEM;
		goto err_priv;
	}

	priv->local.family = -1;
	if (greinfo[IFLA_GRE_LOCAL]) {
		if ((err = addr_init(&priv->local, family, -1, nla_read(greinfo[IFLA_GRE_LOCAL]))))
			goto err_attrs;
		if (!addr_is_zero(&priv->local))
			if_add_config(entry, "local", "%s", priv->local.formatted);
	}

	if (greinfo[IFLA_GRE_REMOTE]) {
		struct addr addr;
		if ((err = addr_init(&addr, family, -1, nla_read(greinfo[IFLA_GRE_REMOTE]))))
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

	free(greinfo);
	return 0;

err_attrs:
	free(greinfo);
err_priv:
	free(priv);
	return err;
}

static int gre_netlink(struct if_entry *entry, struct nlattr **linkinfo)
{
	return gre_common_netlink(AF_INET, entry, linkinfo);
}

static int gre6_netlink(struct if_entry *entry, struct nlattr **linkinfo)
{
	return gre_common_netlink(AF_INET6, entry, linkinfo);
}

static int gre_post(struct if_entry *entry, _unused struct list *netns_list)
{
	struct gre_priv *priv;
	struct if_entry *ife;

	priv = (struct gre_priv *) entry->handler_private;
	if (priv->local.family >= 0) {
		struct netns_entry *ns = entry->link_net ? : entry->ns;

		if ((ife = tunnel_find_addr(ns, &priv->local))) {
			link_set(ife, entry);
			entry->flags |= IF_LINK_WEAK;
		}
	}

	return 0;
}
