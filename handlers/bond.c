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

#include "bond.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "../handler.h"
#include "../if.h"
#include "../netlink.h"
#include "../sysfs.h"
#include "../utils.h"

#include "../compat.h"

static const char *bond_mode_name[] = {
	"",
	"balance-rr",
	"active-backup",
	"balance-xor",
	"broadcast",
	"802.3ad",
	"balance-tlb",
	"balance-alb",
};

struct bond_private {
	uint8_t mode;
	unsigned int active_slave_index;
	char *active_slave_name;
};

static int bond_netlink(struct if_entry *entry, struct nlattr **linkinfo);
static int bond_scan(struct if_entry *entry);
static int bond_post(struct if_entry *entry, struct list *netns_list);
static void bond_cleanup(struct if_entry *entry);

static struct if_handler h_bond = {
	.driver = "bonding",
	.private_size = sizeof(struct bond_private),
	.netlink = bond_netlink,
	.scan = bond_scan,
	.post = bond_post,
	.cleanup = bond_cleanup,
};

void handler_bond_register(void)
{
	if_handler_register(&h_bond);
}

static int bond_netlink(struct if_entry *entry, struct nlattr **linkinfo)
{
	struct bond_private *priv = entry->handler_private;
	struct nlattr *bondinfo[IFLA_BOND_MAX + 1];

	if (!linkinfo || !linkinfo[IFLA_INFO_DATA])
		return ENOENT;
	nla_parse_nested(bondinfo, IFLA_BOND_MAX, linkinfo[IFLA_INFO_DATA]);

	if (bondinfo[IFLA_BOND_MODE]) {
		priv->mode = nla_read_u8(bondinfo[IFLA_BOND_MODE]) + 1;
		if (priv->mode > ARRAY_SIZE(bond_mode_name))
			priv->mode = 0;
	}

	if (bondinfo[IFLA_BOND_ACTIVE_SLAVE]) {
		priv->active_slave_index = nla_read_u32(bondinfo[IFLA_BOND_ACTIVE_SLAVE]);
	}

	return 0;
}

static ssize_t bond_get_sysfs(char **dest, struct if_entry *entry, const char *prop)
{
	char *path;
	ssize_t len;

	*dest = NULL;
	if (asprintf(&path, "class/net/%s/bonding/%s", entry->if_name, prop) < 0)
		return -errno;

	len = sysfs_readfile(dest, path);
	free(path);
	if (len < 0) {
		if (len == -ENOENT)
			return 0;
		return len;
	}

	if (len == 0)
		free(dest);

	return len;
}

static int match_active_slave(struct if_entry *entry, struct if_entry *master)
{
	struct bond_private *priv = master->handler_private;

	if (priv->active_slave_index && entry->if_index != priv->active_slave_index)
		return 0;
	if (priv->active_slave_name && strcmp(entry->if_name, priv->active_slave_name))
		return 0;
	return 1;
}

static int bond_scan(struct if_entry *entry)
{
	char *dest, *tmp;
	struct bond_private *priv = entry->handler_private;

	if (!priv->mode) {
		if (bond_get_sysfs(&dest, entry, "mode") > 0) {
			tmp = index(dest, ' ');
			if (tmp)
				priv->mode = atoi(tmp + 1) + 1;
			free(dest);
		}
		if (priv->mode > ARRAY_SIZE(bond_mode_name))
			priv->mode = 0;
	}

	if (!priv->active_slave_index)
		bond_get_sysfs(&priv->active_slave_name, entry, "active_slave");

	return 0;
}
static int bond_post(struct if_entry *entry, _unused struct list *netns_list)
{
	struct bond_private *priv = entry->handler_private;
	struct if_entry *slave;

	if (priv->mode && *bond_mode_name[priv->mode])
		if_add_config(entry, "mode", "%s", bond_mode_name[priv->mode]);

	if (priv->active_slave_index || priv->active_slave_name) {
		list_for_each_member(slave, entry->rev_master, rev_master_node) {
			if (match_active_slave(slave, entry)) {
				entry->active_slave = slave;
				if_add_state(entry, "active slave", "%s", slave->if_name);
			} else {
				slave->flags |= IF_PASSIVE_SLAVE;
			}
		}
	}
	return 0;
}

static void bond_cleanup(struct if_entry *entry)
{
	struct bond_private *priv = entry->handler_private;

	if (priv->active_slave_name)
		free(priv->active_slave_name);
}
