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

#include "ipxipy.h"
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

static int ipip_netlink(struct if_entry *entry, struct nlattr **linkinfo);
static int ipxip6_netlink(struct if_entry *entry, struct nlattr **linkinfo);
static int sit_netlink(struct if_entry *entry, struct nlattr **linkinfo);
static int ipxipy_post(struct if_entry *entry, struct list *netns_list);

struct ipxipy_priv {
	struct addr local;
};

static struct if_handler h_ipip = {
	.driver = "ipip",
	.private_size = sizeof(struct ipxipy_priv),
	.netlink = ipip_netlink,
	.post = ipxipy_post,
};

static struct if_handler h_sit = {
	.driver = "sit",
	.private_size = sizeof(struct ipxipy_priv),
	.netlink = sit_netlink,
	.post = ipxipy_post,
};

static struct if_handler h_ip6tnl = {
	.driver = "ip6tnl",
	.private_size = sizeof(struct ipxipy_priv),
	.netlink = ipxip6_netlink,
	.post = ipxipy_post,
};

void handler_ipxipy_register(void)
{
	if_handler_register(&h_ipip);
	if_handler_register(&h_sit);
	if_handler_register(&h_ip6tnl);
}

static int ipxipy_netlink(int family, struct if_entry *entry, struct nlattr **linkinfo)
{
	struct nlattr **info;
	struct ipxipy_priv *priv;
	int err;

	if (!linkinfo || !linkinfo[IFLA_INFO_DATA])
		return ENOENT;

	priv = calloc(1, sizeof(struct ipxipy_priv));
	if (!priv)
		return ENOMEM;
	entry->handler_private = priv;

	info = nla_nested_attrs(linkinfo[IFLA_INFO_DATA], IFLA_IPTUN_MAX);
	if (!info) {
		err = ENOMEM;
		goto err_priv;
	}

	priv->local.family = -1;
	if (info[IFLA_IPTUN_LOCAL]) {
		if ((err = addr_init(&priv->local, family, -1, nla_read(info[IFLA_IPTUN_LOCAL]))))
			goto err_attrs;
		if (!addr_is_zero(&priv->local))
			if_add_config(entry, "local", "%s", priv->local.formatted);
	}

	if (info[IFLA_IPTUN_REMOTE]) {
		struct addr addr;
		if ((err = addr_init(&addr, family, -1, nla_read(info[IFLA_IPTUN_REMOTE]))))
			goto err_attrs;
		if (!addr_is_zero(&addr))
			if_add_config(entry, "remote", "%s", addr.formatted);
		addr_destruct(&addr);
	}

	if (info[IFLA_IPTUN_LINK])
		entry->link_index = nla_read_u8(info[IFLA_IPTUN_LINK]);

	if (family == AF_INET6 && info[IFLA_IPTUN_PROTO]) {
		switch (nla_read_u8(info[IFLA_IPTUN_PROTO])) {
		case IPPROTO_IPIP:
			if_add_config(entry, "proto", "ipip6");
			break;
		case IPPROTO_IPV6:
			if_add_config(entry, "proto", "ip6ip6");
			break;
		case 0:
			if_add_config(entry, "proto", "any");
			break;
		}
	}

	free(info);
	return 0;

err_attrs:
	free(info);
err_priv:
	free(priv);
	return err;
}

static int ipip_netlink(struct if_entry *entry, struct nlattr **linkinfo)
{
	return ipxipy_netlink(AF_INET, entry, linkinfo);
}

static int sit_netlink(struct if_entry *entry, struct nlattr **linkinfo)
{
	return ipxipy_netlink(AF_INET, entry, linkinfo);
}

static int ipxip6_netlink(struct if_entry *entry, struct nlattr **linkinfo)
{
	return ipxipy_netlink(AF_INET6, entry, linkinfo);
}

static int ipxipy_post(struct if_entry *entry, _unused struct list *netns_list)
{
	struct ipxipy_priv *priv;
	struct if_entry *ife;

	priv = (struct ipxipy_priv *) entry->handler_private;
	if (priv->local.family >= 0) {
		struct netns_entry *ns = entry->link_net ? : entry->ns;

		if ((ife = tunnel_find_addr(ns, &priv->local))) {
			link_set(ife, entry);
			entry->flags |= IF_LINK_WEAK;
		}
	}

	return 0;
}
