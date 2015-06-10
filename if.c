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
#include "netlink.h"
#include "utils.h"
#include "if.h"

#include "compat.h"

static void fill_if_link(struct if_entry *dest, struct nlmsghdr *n)
{
	struct ifinfomsg *ifi = NLMSG_DATA(n);
	struct rtattr *tb[IFLA_MAX + 1];
	int len = n->nlmsg_len;

	if (n->nlmsg_type != RTM_NEWLINK)
		return;
	len -= NLMSG_LENGTH(sizeof(*ifi));
	if (len < 0)
		return;
	rtnl_parse(tb, IFLA_MAX, IFLA_RTA(ifi), len);
	if (tb[IFLA_IFNAME] == NULL)
		return;
	dest->if_index = ifi->ifi_index;
	dest->if_name = strdup(RTA_DATA(tb[IFLA_IFNAME]));
	if (ifi->ifi_flags & IFF_UP) {
		dest->flags |= IF_UP;
		if (ifi->ifi_flags & IFF_RUNNING)
			dest->flags |= IF_HAS_LINK;
	}
	if (tb[IFLA_MASTER])
		dest->master_index = *(int *)RTA_DATA(tb[IFLA_MASTER]);
	if (tb[IFLA_LINK]) {
		dest->link_index = *(int*)RTA_DATA(tb[IFLA_LINK]);
		if (tb[IFLA_LINK_NETNSID])
			dest->link_netnsid = *(int*)RTA_DATA(tb[IFLA_LINK_NETNSID]);
	}
	if (tb[IFLA_MTU])
		dest->mtu = *(int *)RTA_DATA(tb[IFLA_MTU]);

	if (ifi->ifi_flags & IFF_LOOPBACK) {
		dest->driver = strdup("loopback");
		dest->flags |= IF_LOOPBACK;
	} else
		dest->driver = ethtool_driver(dest->if_name);

	handler_netlink(dest, tb);
}

static int store_addr(struct addr *dest, const struct ifaddrmsg *ifa,
		      const struct rtattr *rta)
{
	char buf[64];
	unsigned int len = RTA_PAYLOAD(rta);

	dest->family = ifa->ifa_family;
	dest->prefixlen = ifa->ifa_prefixlen;
	dest->raw = malloc(len);
	if (!dest->raw)
		return ENOMEM;
	memcpy(dest->raw, RTA_DATA(rta), len);

	if (!inet_ntop(ifa->ifa_family, RTA_DATA(rta), buf, sizeof(buf)))
		return errno;
	len = strlen(buf);
	snprintf(buf + len, sizeof(buf) - len, "/%d", ifa->ifa_prefixlen);
	dest->formatted = strdup(buf);
	if (!dest->formatted)
		return ENOMEM;
	return 0;
}

static int fill_if_addr(struct if_entry *dest, struct nlmsg_entry *ainfo)
{
	struct if_addr_entry *entry, *ptr = NULL;
	struct nlmsghdr *n;
	struct ifaddrmsg *ifa;
	struct rtattr *rta_tb[IFA_MAX + 1];
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
		rtnl_parse(rta_tb, IFA_MAX, IFA_RTA(ifa), len);
		if (!rta_tb[IFA_LOCAL] && !rta_tb[IFA_ADDRESS])
			/* don't care about broadcast and anycast adresses */
			continue;

		entry = calloc(sizeof(struct if_addr_entry), 1);
		if (!entry)
			return ENOMEM;

		if (!rta_tb[IFA_LOCAL]) {
			rta_tb[IFA_LOCAL] = rta_tb[IFA_ADDRESS];
			rta_tb[IFA_ADDRESS] = NULL;
		}
		if ((err = store_addr(&entry->addr, ifa, rta_tb[IFA_LOCAL])))
			return err;
		if (rta_tb[IFA_ADDRESS] &&
		    memcmp(RTA_DATA(rta_tb[IFA_ADDRESS]), RTA_DATA(rta_tb[IFA_LOCAL]),
			   ifa->ifa_family == AF_INET ? 4 : 16)) {
			if ((err = store_addr(&entry->peer, ifa, rta_tb[IFA_ADDRESS])))
				return err;
		}

		if (!ptr)
			dest->addr = entry;
		else
			ptr->next = entry;
		ptr = entry;
	}
	return 0;
}

static struct if_entry *if_alloc(void)
{
	struct if_entry *entry;

	entry = calloc(sizeof(struct if_entry), 1);
	if (!entry)
		return NULL;
	entry->link_netnsid = -1;
	entry->peer_netnsid = -1;
	return entry;
}

int if_list(struct if_entry **result, struct netns_entry *ns)
{
	struct rtnl_handle hnd;
	struct nlmsg_entry *linfo, *ainfo, *l;
	struct if_entry *entry, *ptr = NULL;
	int err;

	*result = NULL;

	if ((err = rtnl_open(&hnd)))
		return err;
	err = rtnl_dump(&hnd, AF_UNSPEC, RTM_GETLINK, &linfo);
	if (err)
		return err;
	err = rtnl_dump(&hnd, AF_UNSPEC, RTM_GETADDR, &ainfo);
	if (err)
		return err;

	for (l = linfo; l; l = l->next) {
		entry = if_alloc();
		if (!entry)
			return ENOMEM;
		entry->ns = ns;
		fill_if_link(entry, &l->h);
		if ((err = fill_if_addr(entry, ainfo)))
			return err;
		if ((err = handler_scan(entry)))
			return err;
		if (!ptr)
			*result = entry;
		else
			ptr->next = entry;
		ptr = entry;
	}

	nlmsg_free(linfo);
	nlmsg_free(ainfo);
	rtnl_close(&hnd);
	return 0;
}

static void if_addr_destruct(struct if_addr_entry *entry)
{
	free(entry->addr.raw);
	free(entry->addr.formatted);
	free(entry->peer.raw);
	free(entry->peer.formatted);
}

static void if_list_destruct(struct if_entry *entry)
{
	handler_cleanup(entry);
	free(entry->internal_ns);
	free(entry->if_name);
	free(entry->edge_label);
	label_free(entry->label);
	list_free(entry->addr, (destruct_f)if_addr_destruct);
	list_free(entry->rev_master, NULL);
}

void if_list_free(struct if_entry *list)
{
	list_free(list, (destruct_f)if_list_destruct);
}

void if_append(struct if_entry **list, struct if_entry *item)
{
	struct if_entry *ptr = *list;

	item->next = NULL;
	if (!ptr) {
		*list = item;
		return;
	}
	while (ptr->next)
		ptr = ptr->next;
	ptr->next = item;
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

int if_list_build_rev(struct if_entry *list)
{
	while (list) {
		if (list->master) {
			struct if_list_entry *le;

			le = malloc(sizeof(*le));
			if (!le)
				return ENOMEM;
			le->entry = list;
			le->next = list->master->rev_master;
			list->master->rev_master = le;
		}
		if (list->link)
			list->link->rev_link = list->link;
		list = list->next;
	}
	return 0;
}
