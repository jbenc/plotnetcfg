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

static int route_parse_metrics(struct list *metrics, struct nlattr *mxrta)
{
	struct nlattr **tb;
	struct rtmetric *rtm;
	int i;

	tb = nla_nested_attrs(mxrta, RTAX_MAX);
	if (!tb)
		return ENOMEM;

	for (i = 1; i <= RTAX_MAX; i++) {
		if (!tb[i] || i == RTAX_CC_ALGO)
			continue;

		rtm = calloc(1, sizeof(struct rtmetric));
		if (!rtm) {
			free(tb);
			return ENOMEM;
		}

		rtm->type = i;
		rtm->value = nla_read_u32(tb[i]);
		list_append(metrics, node(rtm));
	}

	return 0;
}

int route_create_netlink(struct route **rte, struct nlmsg *msg)
{
	struct rtmsg *rtmsg;
	struct nlattr **tb;
	struct route *r;
	int err;

	*rte = NULL;

	if (nlmsg_get_hdr(msg)->nlmsg_type != RTM_NEWROUTE)
		return ENOENT;

	rtmsg = nlmsg_get(msg, sizeof(*rtmsg));
	if (!rtmsg)
		return ENOENT;

	r = calloc(1, sizeof(struct route));
	if (!r)
		return ENOMEM;

	r->family = rtmsg->rtm_family;
	r->protocol = rtmsg->rtm_protocol;
	r->scope = rtmsg->rtm_scope;
	r->tos = rtmsg->rtm_tos;
	r->type = rtmsg->rtm_type;

	tb = nlmsg_attrs(msg, RTA_MAX);
	if (!tb) {
		err = ENOMEM;
		goto err_rte;
	}

	if (tb[RTA_TABLE])
		r->table_id = nla_read_u32(tb[RTA_TABLE]);
	else
		r->table_id = rtmsg->rtm_table;

	if (tb[RTA_SRC])
		addr_init(&r->src, r->family, rtmsg->rtm_src_len,
			  nla_read(tb[RTA_SRC]));
	if (tb[RTA_DST])
		addr_init(&r->dst, r->family, rtmsg->rtm_dst_len,
			  nla_read(tb[RTA_DST]));
	if (tb[RTA_GATEWAY])
		addr_init(&r->gw, r->family, -1,
			  nla_read(tb[RTA_GATEWAY]));
	if (tb[RTA_PREFSRC])
		addr_init(&r->prefsrc, r->family, -1,
			  nla_read(tb[RTA_PREFSRC]));

	if (tb[RTA_OIF])
		r->oifindex = nla_read_u32(tb[RTA_OIF]);
	if (tb[RTA_IIF])
		r->iifindex = nla_read_u32(tb[RTA_IIF]);
	if (tb[RTA_PRIORITY])
		r->priority = nla_read_u32(tb[RTA_PRIORITY]);


	list_init(&r->metrics);
	if (tb[RTA_METRICS])
		if ((err = route_parse_metrics(&r->metrics, tb[RTA_METRICS])))
			goto err_tb;

	*rte = r;
	free(tb);
	return 0;

err_tb:
	free(tb);
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
	list_init(&rt->routes);

	*rtd = rt;
	return 0;
}

static struct if_entry *find_if_by_ifindex(struct list *list, unsigned int ifindex)
{
	struct if_entry *entry;

	list_for_each(entry, *list)
		if (entry->if_index == ifindex)
			return entry;
	return NULL;
}


int route_scan(struct netns_entry *ns)
{
	struct nl_handle hnd;
	struct nlmsg *req, *resp;
	struct rtmsg msg = {
		.rtm_table = RT_TABLE_UNSPEC,
		.rtm_protocol = RTPROT_UNSPEC,
	};
	struct rtable *tables [256];
	struct route *r;
	int err, i;

	memset(tables, 0, sizeof(tables));
	list_init(&ns->rtables);

	if ((err = rtnl_open(&hnd)))
		return err;

	req = nlmsg_new(RTM_GETROUTE, NLM_F_DUMP);
	if (!req) {
		err = ENOMEM;
		goto err_handle;
	}
	err = nlmsg_put(req, &msg, sizeof(msg));
	if (err)
		goto err_req;

	err = nl_exchange(&hnd, req, &resp);
	if (err)
		goto err_req;

	for_each_nlmsg(nle, resp) {
		if ((err = route_create_netlink(&r, nle)))
			goto err_resp;

		r->oif = find_if_by_ifindex(&ns->ifaces, r->oifindex);
		r->iif = find_if_by_ifindex(&ns->ifaces, r->iifindex);

		if (!tables[r->table_id])
			if ((err = rtable_create(&tables[r->table_id], r->table_id)))
				goto err_route;

		list_append(&tables[r->table_id]->routes, node(r));
		r = NULL;
	}

	for (i = 255; i >= 0; i--) {
		if (tables[i])
			list_append(&ns->rtables, node(tables[i]));
	}

err_route:
	if (r)
		free(r);
err_resp:
	nlmsg_free(resp);
err_req:
	nlmsg_free(req);
err_handle:
	nl_close(&hnd);
	return err;
}

static void route_destruct(struct route *r)
{
	list_free(&r->metrics, NULL);
}

static void rtable_free(struct rtable *rt)
{
	list_free(&rt->routes, (destruct_f) route_destruct);
}

static void route_cleanup(struct netns_entry *entry)
{
	list_free(&entry->rtables, (destruct_f) rtable_free);
}
