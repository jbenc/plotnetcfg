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

#include "macsec.h"
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include "../handler.h"
#include "../if.h"
#include "../netlink.h"

static int macsec_netlink(struct if_entry *entry, struct nlattr **linkinfo);

static struct if_handler h_macsec = {
	.driver = "macsec",
	.netlink = macsec_netlink,
};

void handler_macsec_register(void)
{
	if_handler_register(&h_macsec);
}

static int macsec_netlink(struct if_entry *entry, struct nlattr **linkinfo)
{
	struct nlattr **macsecinfo;
	int err = 0;
	uint64_t sci;

	if (!linkinfo || !linkinfo[IFLA_INFO_DATA])
		return ENOENT;

	macsecinfo = nla_nested_attrs(linkinfo[IFLA_INFO_DATA], IFLA_MACSEC_MAX);
	if (!macsecinfo)
		return ENOMEM;

	if (!macsecinfo[IFLA_MACSEC_SCI]) {
		err = ENOENT;
		goto err_attrs;
	}

	sci = nla_read_be64(macsecinfo[IFLA_MACSEC_SCI]);
	if (asprintf(&entry->edge_label, "sci %" PRIx64, sci) < 0) {
		err = ENOMEM;
		goto err_attrs;
	}

err_attrs:
	free(macsecinfo);
	return err;
}
