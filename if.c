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

#include "if.h"
#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "ethtool.h"
#include "handler.h"
#include "label.h"
#include "list.h"
#include "netlink.h"
#include "netns.h"
#include "utils.h"

#include "compat.h"

static int fill_if_link(struct if_entry *dest, struct nlmsg *msg)
{
	struct ifinfomsg *ifi;
	struct nlattr **tb, **linkinfo = NULL;
	int err;

	if (nlmsg_get_hdr(msg)->nlmsg_type != RTM_NEWLINK)
		return ENOENT;
	ifi = nlmsg_get(msg, sizeof(*ifi));
	if (!ifi)
		return ENOENT;
	tb = nlmsg_attrs(msg, IFLA_MAX);
	if (!tb)
		return ENOMEM;
	if (!tb[IFLA_IFNAME]) {
		err = ENOENT;
		goto out;
	}
	dest->if_index = ifi->ifi_index;
	dest->if_name = strdup(nla_read_str(tb[IFLA_IFNAME]));
	if (!dest->if_name) {
		err = ENOMEM;
		goto err_ifname;
	}
	if (ifi->ifi_flags & IFF_UP) {
		dest->flags |= IF_UP;
		if (ifi->ifi_flags & IFF_RUNNING)
			dest->flags |= IF_HAS_LINK;
	}
	if (tb[IFLA_MASTER])
		dest->master_index = nla_read_u32(tb[IFLA_MASTER]);
	if (tb[IFLA_LINK]) {
		dest->link_index = nla_read_u32(tb[IFLA_LINK]);
		if (tb[IFLA_LINK_NETNSID])
			dest->link_netnsid = nla_read_s32(tb[IFLA_LINK_NETNSID]);
	}
	if (tb[IFLA_MTU])
		dest->mtu = nla_read_u32(tb[IFLA_MTU]);
	if (tb[IFLA_LINKINFO]) {
		linkinfo = nla_nested_attrs(tb[IFLA_LINKINFO], IFLA_INFO_MAX);
		if (!linkinfo) {
			err = ENOMEM;
			goto err_ifname;
		}
	}

	if (tb[IFLA_ADDRESS]) {
		err = mac_addr_fill_netlink(&dest->mac_addr, tb[IFLA_ADDRESS]);
		if (err)
			goto err_ifname;
	}

	if (ifi->ifi_flags & IFF_LOOPBACK) {
		dest->driver = strdup("loopback");
		dest->flags |= IF_LOOPBACK;
	} else
		dest->driver = ethtool_driver(dest->if_name);
	if (!dest->driver) {
		/* No ethtool ops available, try IFLA_INFO_KIND */
		if (tb[IFLA_LINKINFO] && linkinfo && linkinfo[IFLA_INFO_KIND])
			dest->driver = strdup(RTA_DATA(linkinfo[IFLA_INFO_KIND]));
	}
	if (!dest->driver) {
		/* Allow the program to continue at least with generic stuff
		 * as there may be interfaces that do not implement any of
		 * the mechanisms for driver detection that we use */
		dest->driver = strdup("unknown driver, please report a bug");
	}

	if ((err = if_handler_init(dest)))
		goto err_driver;

	if ((err = if_handler_netlink(dest, linkinfo)))
		if (err != ENOENT)
			goto err_driver;

	err = 0;
	goto out;

err_driver:
	free(dest->driver);
	dest->driver = NULL;
err_ifname:
	free(dest->if_name);
	dest->if_name = NULL;
out:
	free(tb);
	free(linkinfo);
	return err;
}

static int fill_if_addr(struct if_entry *dest, struct nlmsg *alist)
{
	struct if_addr *entry;
	struct ifaddrmsg *ifa;
	struct nlattr **rta_tb;
	int err;

	for_each_nlmsg(ainfo, alist) {
		rta_tb = NULL;
		err = 0;
		ifa = nlmsg_get(ainfo, sizeof(*ifa));
		if (!ifa)
			return ENOENT;
		if (ifa->ifa_index != dest->if_index)
			goto skip;
		if (nlmsg_get_hdr(ainfo)->nlmsg_type != RTM_NEWADDR)
			goto skip;
		if (ifa->ifa_family != AF_INET &&
		    ifa->ifa_family != AF_INET6)
			/* only IP addresses supported (at least for now) */
			goto skip;
		rta_tb = nlmsg_attrs(ainfo, IFA_MAX);
		if (!rta_tb) {
			err = ENOMEM;
			goto skip;
		}
		if (!rta_tb[IFA_LOCAL] && !rta_tb[IFA_ADDRESS])
			/* don't care about broadcast and anycast adresses */
			goto skip;

		entry = calloc(1, sizeof(struct if_addr));
		if (!entry) {
			err = ENOMEM;
			goto skip;
		}

		list_append(&dest->addr, node(entry));

		if (!rta_tb[IFA_LOCAL]) {
			rta_tb[IFA_LOCAL] = rta_tb[IFA_ADDRESS];
			rta_tb[IFA_ADDRESS] = NULL;
		}
		if ((err = addr_init_netlink(&entry->addr, ifa, rta_tb[IFA_LOCAL])))
			goto skip;
		if (rta_tb[IFA_ADDRESS] &&
		    memcmp(nla_read(rta_tb[IFA_ADDRESS]), nla_read(rta_tb[IFA_LOCAL]),
			   ifa->ifa_family == AF_INET ? 4 : 16)) {
			if ((err = addr_init_netlink(&entry->peer, ifa, rta_tb[IFA_ADDRESS])))
				goto skip;
		}
skip:
		nlmsg_unget(ainfo, sizeof(*ifa));
		if (rta_tb)
			free(rta_tb);
		if (err)
			return err;
	}
	return 0;
}

struct if_entry *if_create(void)
{
	struct if_entry *entry;

	entry = calloc(1, sizeof(struct if_entry));
	if (!entry)
		return NULL;

	list_init(&entry->addr);
	list_init(&entry->rev_master);
	list_init(&entry->rev_link);
	list_init(&entry->properties);
	entry->link_netnsid = -1;
	entry->peer_netnsid = -1;
	mac_addr_init(&entry->mac_addr);
	return entry;
}

int if_list(struct list *result, struct netns_entry *ns)
{
	struct nl_handle hnd;
	struct nlmsg *linfo, *ainfo;
	struct if_entry *entry;
	int err;

	list_init(result);

	if ((err = rtnl_open(&hnd)))
		return err;
	err = rtnl_ifi_dump(&hnd, RTM_GETLINK, AF_UNSPEC, &linfo);
	if (err)
		goto out_close;
	err = rtnl_ifi_dump(&hnd, RTM_GETADDR, AF_UNSPEC, &ainfo);
	if (err)
		goto out_linfo;

	for_each_nlmsg(l, linfo) {
		entry = if_create();
		if (!entry) {
			err = ENOMEM;
			goto out_ainfo;
		}
		list_append(result, node(entry));
		entry->ns = ns;
		if ((err = fill_if_link(entry, l)))
			goto out_ainfo;
		if ((err = fill_if_addr(entry, ainfo)))
			goto out_ainfo;
		if ((err = if_handler_scan(entry)))
			goto out_ainfo;
	}
	err = 0;

out_ainfo:
	nlmsg_free(ainfo);
out_linfo:
	nlmsg_free(linfo);
out_close:
	nl_close(&hnd);
	return err;
}

static void if_addr_destruct(struct if_addr *entry)
{
	addr_destruct(&entry->addr);
	addr_destruct(&entry->peer);
}

static void if_list_destruct(struct if_entry *entry)
{
	if_handler_cleanup(entry);
	free(entry->internal_ns);
	free(entry->if_name);
	free(entry->edge_label);
	free(entry->driver);
	mac_addr_destruct(&entry->mac_addr);
	label_free_property(&entry->properties);
	list_free(&entry->addr, (destruct_f) if_addr_destruct);
}

void if_list_free(struct list *list)
{
	list_free(list, (destruct_f) if_list_destruct);
}

int if_add_warning(struct if_entry *entry, char *fmt, ...)
{
	va_list ap;
	char *warn;
	int err = ENOMEM;

	va_start(ap, fmt);
	entry->warnings++;
	if (vasprintf(&warn, fmt, ap) < 0)
		goto out;
	err = label_add(&entry->ns->warnings, "%s: %s", ifstr(entry), warn);
	free(warn);
out:
	va_end(ap);
	return err;
}
