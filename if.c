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

static int fill_if_link(struct if_entry *dest, struct nlmsghdr *n)
{
	struct ifinfomsg *ifi = NLMSG_DATA(n);
	struct nlattr *tb[IFLA_MAX + 1];
	struct nlattr *linkinfo[IFLA_INFO_MAX + 1];
	int len = n->nlmsg_len;
	int err;

	if (n->nlmsg_type != RTM_NEWLINK)
		return ENOENT;
	len -= NLMSG_LENGTH(sizeof(*ifi));
	if (len < 0)
		return ENOENT;
	nla_parse(tb, IFLA_MAX, IFLA_RTA(ifi), len);
	if (tb[IFLA_IFNAME] == NULL)
		return ENOENT;
	dest->if_index = ifi->ifi_index;
	dest->if_name = strdup(nla_read_str(tb[IFLA_IFNAME]));
	if (!dest->if_name)
		return ENOMEM;
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
	if (tb[IFLA_LINKINFO])
		nla_parse_nested(linkinfo, IFLA_INFO_MAX, tb[IFLA_LINKINFO]);

	if (tb[IFLA_ADDRESS]) {
		err = mac_addr_fill_netlink(&dest->mac_addr, tb[IFLA_ADDRESS]);
		if (err)
			return err;
	}

	if (ifi->ifi_flags & IFF_LOOPBACK) {
		dest->driver = strdup("loopback");
		dest->flags |= IF_LOOPBACK;
	} else
		dest->driver = ethtool_driver(dest->if_name);
	if (!dest->driver) {
		/* No ethtool ops available, try IFLA_INFO_KIND */
		if (tb[IFLA_LINKINFO] && linkinfo[IFLA_INFO_KIND])
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

	if ((err = if_handler_netlink(dest, tb[IFLA_LINKINFO] ? linkinfo : NULL)))
		if (err != ENOENT)
			goto err_driver;

	return 0;

err_driver:
	free(dest->driver);
	dest->driver = NULL;
	free(dest->if_name);
	dest->if_name = NULL;
	return err;
}

static int fill_if_addr(struct if_entry *dest, struct nlmsg_entry *ainfo)
{
	struct if_addr *entry;
	struct nlmsghdr *n;
	struct ifaddrmsg *ifa;
	struct nlattr *rta_tb[IFA_MAX + 1];
	int len, err;

	for (; ainfo; ainfo = ainfo->next) {
		n = &ainfo->h;
		ifa = NLMSG_DATA(n);
		if (ifa->ifa_index != dest->if_index)
			continue;
		if (n->nlmsg_type != RTM_NEWADDR)
			continue;
		len = n->nlmsg_len - NLMSG_LENGTH(sizeof(*ifa));
		if (len < 0)
			continue;
		if (ifa->ifa_family != AF_INET &&
		    ifa->ifa_family != AF_INET6)
			/* only IP addresses supported (at least for now) */
			continue;
		nla_parse(rta_tb, IFA_MAX, IFA_RTA(ifa), len);
		if (!rta_tb[IFA_LOCAL] && !rta_tb[IFA_ADDRESS])
			/* don't care about broadcast and anycast adresses */
			continue;

		entry = calloc(1, sizeof(struct if_addr));
		if (!entry)
			return ENOMEM;

		if (!rta_tb[IFA_LOCAL]) {
			rta_tb[IFA_LOCAL] = rta_tb[IFA_ADDRESS];
			rta_tb[IFA_ADDRESS] = NULL;
		}
		if ((err = addr_init_netlink(&entry->addr, ifa, rta_tb[IFA_LOCAL])))
			return err;
		if (rta_tb[IFA_ADDRESS] &&
		    memcmp(nla_read(rta_tb[IFA_ADDRESS]), nla_read(rta_tb[IFA_LOCAL]),
			   ifa->ifa_family == AF_INET ? 4 : 16)) {
			if ((err = addr_init_netlink(&entry->peer, ifa, rta_tb[IFA_ADDRESS])))
				return err;
		}

		list_append(&dest->addr, node(entry));
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
	struct nlmsg_entry *linfo, *ainfo, *l;
	struct if_entry *entry;
	int err;

	list_init(result);

	if ((err = rtnl_open(&hnd)))
		return err;
	err = rtnl_dump(&hnd, AF_UNSPEC, RTM_GETLINK, &linfo);
	if (err)
		goto out_close;
	err = rtnl_dump(&hnd, AF_UNSPEC, RTM_GETADDR, &ainfo);
	if (err)
		goto out_linfo;

	for (l = linfo; l; l = l->next) {
		entry = if_create();
		if (!entry) {
			err = ENOMEM;
			goto out_ainfo;
		}
		list_append(result, node(entry));
		entry->ns = ns;
		if ((err = fill_if_link(entry, &l->h)))
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
