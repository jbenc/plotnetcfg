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

#include "geneve.h"
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

#define GENEVE_DEFAULT_PORT 6081

struct geneve_priv {
	int flags;
};

static int geneve_netlink(struct if_entry *entry, struct nlattr **linkinfo);

static struct if_handler h_geneve = {
	.driver = "geneve",
	.private_size = sizeof(struct geneve_priv),
	.netlink = geneve_netlink,
};

void handler_geneve_register(void)
{
	if_handler_register(&h_geneve);
}

static int geneve_netlink(struct if_entry *entry, struct nlattr **linkinfo)
{
	struct nlattr **geneveinfo;
	uint16_t port;
	struct geneve_priv *priv;
	int err;

	priv = calloc(1, sizeof(struct geneve_priv));
	if (!priv) {
		err = ENOMEM;
		goto err;
	}
	entry->handler_private = priv;

	if (!linkinfo || !linkinfo[IFLA_INFO_DATA]) {
		err = ENOENT;
		goto err_priv;
	}
	geneveinfo = nla_nested_attrs(linkinfo[IFLA_INFO_DATA], IFLA_GENEVE_MAX);
	if (!geneveinfo) {
		err = ENOMEM;
		goto err_priv;
	}

	if (geneveinfo[IFLA_GENEVE_ID])
		if_add_config(entry, "VNI", "%u", nla_read_u32(geneveinfo[IFLA_GENEVE_ID]));

	if (geneveinfo[IFLA_GENEVE_PORT]) {
		port = nla_read_be16(geneveinfo[IFLA_GENEVE_PORT]);
		if (port != GENEVE_DEFAULT_PORT)
			if_add_config(entry, "port", "%u", port);
	}

	if (geneveinfo[IFLA_GENEVE_REMOTE]) {
		struct addr addr;
		if ((err = addr_init(&addr, AF_INET, -1, nla_read(geneveinfo[IFLA_GENEVE_REMOTE]))))
			goto err_attrs;
		if (!addr_is_zero(&addr))
			if_add_config(entry, "remote", "%s", addr.formatted);
		addr_destruct(&addr);
	}

	if (geneveinfo[IFLA_GENEVE_REMOTE6]) {
		struct addr addr;
		if ((err = addr_init(&addr, AF_INET6, -1, nla_read(geneveinfo[IFLA_GENEVE_REMOTE6]))))
			goto err_attrs;
		if (!addr_is_zero(&addr))
			if_add_config(entry, "remote6", "%s", addr.formatted);
		addr_destruct(&addr);
	}

	if (geneveinfo[IFLA_GENEVE_COLLECT_METADATA])
		if_add_config(entry, "mode", "external");

	free(geneveinfo);
	return 0;

err_attrs:
	free(geneveinfo);
err_priv:
	free(priv);
err:
	return err;
}
