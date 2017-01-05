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

#include "route.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../handler.h"
#include "../if.h"
#include "../label.h"
#include "../list.h"
#include "../netlink.h"
#include "../netns.h"
#include "../route.h"

#include "../compat.h"

static int route_scan(struct netns_entry *entry);
static void route_cleanup(struct netns_entry *entry);

static struct netns_handler h_route = {
	.scan = route_scan,
	.cleanup = route_cleanup,
};

void handler_route_register(void)
{
	netns_handler_register(&h_route);
}

static int route_parse_metrics(struct rtmetric **metrics, struct rtattr *mxrta)
{
	struct rtattr *tb [RTAX_MAX + 1];
	struct rtmetric *rtm;
	int i;

	rtnl_parse(tb, RTAX_MAX, RTA_DATA(mxrta), RTA_PAYLOAD(mxrta));

	for (i = 1; i <= RTAX_MAX; i++) {
		if (!tb[i] || i == RTAX_CC_ALGO)
			continue;

		rtm = calloc(1, sizeof(struct rtmetric));
		if (!rtm)
			return ENOMEM;

		rtm->type = i;
		rtm->value = NLA_GET_U32(tb[i]);
		rtm->next = *metrics;
		*metrics = rtm;
	}

	return 0;
}

int route_create_netlink(struct route **rte, struct nlmsghdr *n)
{
	struct rtmsg *rtmsg = NLMSG_DATA(n);
	struct rtattr *tb[RTA_MAX + 1];
	struct route *r;
	int len = n->nlmsg_len;
	int err;

	*rte = NULL;

	if (n->nlmsg_type != RTM_NEWROUTE)
		return ENOENT;

	len -= NLMSG_LENGTH(sizeof(*rtmsg));
	if (len < 0)
		return ENOENT;

	r = calloc(1, sizeof(struct route));
	if (!r)
		return ENOMEM;

	r->family = rtmsg->rtm_family;
	r->protocol = rtmsg->rtm_protocol;
	r->scope = rtmsg->rtm_scope;
	r->tos = rtmsg->rtm_tos;
	r->type = rtmsg->rtm_type;

	rtnl_parse(tb, RTA_MAX, RTM_RTA(rtmsg), len);

	if (tb[RTA_TABLE])
		r->table_id = NLA_GET_U32(tb[RTA_TABLE]);
	else
		r->table_id = rtmsg->rtm_table;

	if (tb[RTA_SRC])
		addr_init(&r->src, r->family, rtmsg->rtm_src_len,
			  RTA_DATA(tb[RTA_SRC]));
	if (tb[RTA_DST])
		addr_init(&r->dst, r->family, rtmsg->rtm_dst_len,
			  RTA_DATA(tb[RTA_DST]));
	if (tb[RTA_GATEWAY])
		addr_init(&r->gw, r->family, -1,
			  RTA_DATA(tb[RTA_GATEWAY]));
	if (tb[RTA_PREFSRC])
		addr_init(&r->prefsrc, r->family, -1,
			  RTA_DATA(tb[RTA_PREFSRC]));

	if (tb[RTA_OIF])
		r->oifindex = NLA_GET_U32(tb[RTA_OIF]);
	if (tb[RTA_IIF])
		r->iifindex = NLA_GET_U32(tb[RTA_IIF]);
	if (tb[RTA_PRIORITY])
		r->priority = NLA_GET_U32(tb[RTA_PRIORITY]);

	if (tb[RTA_METRICS])
		if ((err = route_parse_metrics(&r->metrics, tb[RTA_METRICS])))
			goto err_rte;

	*rte = r;
	return 0;

err_rte:
	free(r);
	return err;
}

static int rtable_create(struct rtable **rtd, int id)
{
	struct rtable *rt;

	rt = calloc(1, sizeof(struct rtable));
	if (!rt)
		return ENOMEM;

	rt->id = id;

	*rtd = rt;
	return 0;
}

static struct if_entry *find_if_by_ifindex(struct if_entry *ife, unsigned int ifindex)
{
	for (; ife; ife = ife->next)
		if (ife->if_index == ifindex)
			return ife;
	return NULL;
}


int route_scan(struct netns_entry *ns)
{
	struct nl_handle hnd;
	struct {
		struct nlmsghdr n;
		struct rtmsg r;
	} req;
	struct rtable *rt, *tables [256];
	struct route *r;
	struct nlmsg_entry *dst, *nle;
	int err, i;

	if ((err = rtnl_open(&hnd)))
		return err;

	memset(&req, 0, sizeof(req));
	memset(&tables, 0, sizeof(tables));
	req.n.nlmsg_len = sizeof(req);
	req.n.nlmsg_type = RTM_GETROUTE;
	req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	req.r.rtm_table = RT_TABLE_UNSPEC;
	req.r.rtm_protocol = RTPROT_UNSPEC;

	if ((err = nl_exchange(&hnd, &req.n, &dst)))
		goto err_handle;

	for (nle = dst; nle; nle = nle->next) {
		if ((err = route_create_netlink(&r, &nle->h)))
			goto err_dst;

		r->oif = find_if_by_ifindex(ns->ifaces, r->oifindex);
		r->iif = find_if_by_ifindex(ns->ifaces, r->iifindex);

		if (!tables[r->table_id])
			if ((err = rtable_create(&tables[r->table_id], r->table_id)))
				goto err_route;

		rt = tables[r->table_id];

		r->next = NULL;
		if (rt->tail)
			rt->tail->next = r;
		else
			rt->routes = r;
		rt->tail = r;
		r = NULL;
	}

	for (i = 255; i >= 0; i--) {
		if (!tables[i])
			continue;
		tables[i]->next = ns->rtables;
		ns->rtables = tables[i];
	}

err_route:
	if (r)
		free(r);
err_dst:
	nlmsg_free(dst);
err_handle:
	nl_close(&hnd);
	return err;
}

static void rtable_free(struct rtable *rt)
{
	if (rt->routes)
		slist_free(rt->routes, NULL);
}

static void route_cleanup(struct netns_entry *entry)
{
	if (entry->rtables)
		slist_free(entry->rtables, (destruct_f) rtable_free);
}
