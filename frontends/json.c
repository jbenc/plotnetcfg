/*
 * This file is a part of plotnetcfg, a tool to visualize network config.
 * Copyright (C) 2015 Red Hat, Inc. -- Jiri Benc <jbenc@redhat.com>
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

#include "json.h"
#include <arpa/inet.h>
#include <jansson.h>
#include <stdio.h>
#include <sys/socket.h>
#include <time.h>
#include "../addr.h"
#include "../frontend.h"
#include "../if.h"
#include "../label.h"
#include "../netns.h"
#include "../route.h"
#include "../utils.h"
#include "../version.h"

static json_t *label_to_array(struct label *entry)
{
	json_t *arr;

	arr = json_array();
	while (entry) {
		json_array_append_new(arr, json_string(entry->text));
		entry = entry->next;
	}
	return arr;
}

static json_t *label_properties_to_object(struct label_property *prop, unsigned int prop_mask)
{
	json_t *jobj;

	jobj = json_object();
	while (prop) {
		if (label_prop_match_mask(prop->type, prop_mask))
			json_object_set_new(jobj, prop->key, json_string(prop->value));
		prop = prop->next;
	}
	return jobj;
}

static json_t *address_family(int family)
{
	switch (family) {
		case AF_INET: return json_string("INET");
		case AF_INET6: return json_string("INET6");
	}
	/* should not happen */
	return json_string("unknown");
}

static json_t *address_to_obj(struct addr *addr)
{
	json_t *obj = json_object();
	json_object_set_new(obj, "family", address_family(addr->family));
	json_object_set_new(obj, "address", json_string(addr->formatted));
	return obj;
}

static json_t *addresses_to_array(struct list *addresses)
{
	json_t *arr, *addr;
	struct if_addr *entry;

	arr = json_array();
	list_for_each(entry, *addresses) {
		addr = address_to_obj(&entry->addr);
		if (entry->peer.formatted)
			json_object_set_new(addr, "peer", address_to_obj(&entry->peer));
		json_array_append_new(arr, addr);
	}
	return arr;
}

static json_t *connection(struct if_entry *target, char *edge_label)
{
	json_t *obj, *arr;

	obj = json_object();
	json_object_set_new(obj, "target", json_string(ifid(target)));

	arr = json_array();
	if (edge_label)
		json_array_append_new(arr, json_string(edge_label));
	json_object_set_new(obj, "info", arr);
	return obj;
}

static json_t *interfaces_to_array(struct list *list, struct output_entry *output_entry)
{
	struct if_entry *entry, *link, *slave;
	json_t *ifarr, *ifobj, *children, *parents, *jconn;
	char *s;

	ifarr = json_object();
	list_for_each(entry, *list) {
		ifobj = json_object();
		json_object_set_new(ifobj, "id", json_string(ifid(entry)));
		json_object_set_new(ifobj, "namespace", json_string(nsid(entry->ns)));
		json_object_set_new(ifobj, "name", json_string(entry->if_name));
		json_object_set_new(ifobj, "driver", json_string(entry->driver ? entry->driver : ""));
		json_object_set_new(ifobj, "info", label_properties_to_object(entry->prop, output_entry->print_mask));
		if (label_prop_match_mask(IF_PROP_CONFIG, output_entry->print_mask)) {
			json_object_set_new(ifobj, "addresses", addresses_to_array(&entry->addr));
			json_object_set_new(ifobj, "mtu", json_integer(entry->mtu));
			if ((entry->flags & IF_LOOPBACK) == 0 && entry->mac_addr.formatted)
				json_object_set_new(ifobj, "mac", json_string(entry->mac_addr.formatted));
		}
		json_object_set_new(ifobj, "type", json_string(entry->flags & IF_INTERNAL ?
							       "internal" :
							       "device"));
		if (entry->flags & IF_INTERNAL)
			s = "none";
		else if (!(entry->flags & IF_UP))
			s = "down";
		else if (!(entry->flags & IF_HAS_LINK))
			s = "up_no_link";
		else
			s = "up";
		if (label_prop_match_mask(IF_PROP_STATE, output_entry->print_mask))
			json_object_set_new(ifobj, "state", json_string(s));
		if (entry->warnings)
			json_object_set_new(ifobj, "warning", json_true());

		parents = json_object();
		if (entry->master) {
			jconn = connection(entry->master,
						      entry->link ? NULL : entry->edge_label);
			json_object_set_new(parents, ifid(entry->master), jconn);
		} else
			list_for_each_member(link, entry->rev_link, rev_link_node) {
				jconn = connection(link, link->edge_label);
				json_object_set_new(parents, ifid(link), jconn);
			}
		if (json_object_size(parents))
			json_object_set(ifobj, "parents", parents);
		json_decref(parents);

		children = json_object();
		if (entry->link) {
			jconn = connection(entry->link, entry->edge_label);
			json_object_set_new(children, ifid(entry->link), jconn);
		} else
			list_for_each_member(slave, entry->rev_master, rev_master_node) {
				jconn = connection(slave, slave->link ? NULL : slave->edge_label);
				json_object_set_new(children, ifid(slave), jconn);
			}
		if (json_object_size(children))
			json_object_set(ifobj, "children", children);
		json_decref(children);

		if (entry->peer)
			json_object_set(ifobj, "peer", connection(entry->peer, NULL));

		json_object_set_new(ifarr, ifid(entry), ifobj);
	}
	return ifarr;
}

static json_t *routes_to_array(struct route *rte)
{
	json_t *ifarr, *ifobj;
	struct rtmetric *rtm;

	ifarr = json_array();
	for (; rte; rte = rte->next) {
		ifobj = json_object();
		if (rte->dst.family)
			json_object_set_new(ifobj, "destination", json_string(rte->dst.formatted));
		json_object_set_new(ifobj, "family", address_family(rte->family));
		if (rte->gw.family)
			json_object_set_new(ifobj, "gateway", json_string(rte->gw.formatted));
		if (rte->iif)
			json_object_set_new(ifobj, "iif", json_string(ifid(rte->iif)));
		if (rte->metrics) {
			json_t *rtmetrics = json_object();

			for (rtm = rte->metrics; rtm; rtm = rtm->next)
				json_object_set_new(rtmetrics, route_metric(rtm->type), json_integer(rtm->value));

			json_object_set_new(ifobj, "metrics", rtmetrics);
		}
		if (rte->oif)
			json_object_set_new(ifobj, "oif", json_string(ifid(rte->oif)));
		json_object_set_new(ifobj, "priority", json_integer(rte->priority));
		json_object_set_new(ifobj, "protocol", json_string(route_protocol(rte->protocol)));
		json_object_set_new(ifobj, "scope", json_string(route_scope(rte->scope)));
		if (rte->src.family)
			json_object_set_new(ifobj, "source", json_string(rte->src.formatted));
		if (rte->prefsrc.family)
			json_object_set_new(ifobj, "preferred-source", json_string(rte->prefsrc.formatted));
		json_object_set_new(ifobj, "tos", json_integer(rte->tos));
		json_object_set_new(ifobj, "type", json_string(route_type(rte->type)));

		json_array_append_new(ifarr, ifobj);
	}

	return ifarr;
}

static json_t *rtables_to_array(struct rtable *rt)
{
	json_t *ifarr, *ifobj;

	ifarr = json_object();
	for (; rt; rt = rt->next) {
		ifobj = json_object();
		json_object_set_new(ifobj, "name", json_string(route_table(rt->id)));
		json_object_set_new(ifobj, "routes", routes_to_array(rt->routes));
		json_object_set_new(ifarr, rtid(rt), ifobj);
	}

	return ifarr;
}

static void json_output(FILE *f, struct netns_entry *root, struct output_entry *output_entry)
{
	struct netns_entry *entry;
	json_t *output, *ns_list, *ns;
	time_t cur;

	time(&cur);
	output = json_object();
	json_object_set_new(output, "format", json_integer(2));
	json_object_set_new(output, "version", json_string(VERSION));
	json_object_set_new(output, "date", json_string(ctime(&cur)));
	json_object_set_new(output, "root", json_string(nsid(root)));
	ns_list = json_object();
	for (entry = root; entry; entry = entry->next) {
		ns = json_object();
		json_object_set_new(ns, "id", json_string(nsid(entry)));
		json_object_set_new(ns, "name", json_string(entry->name ? entry->name : ""));
		json_object_set_new(ns, "interfaces", interfaces_to_array(&entry->ifaces, output_entry));
		json_object_set_new(ns, "routes", rtables_to_array(entry->rtables));
		if (entry->warnings)
			json_object_set_new(ns, "warnings", label_to_array(entry->warnings));
		json_object_set_new(ns_list, nsid(entry), ns);
	}
	json_object_set_new(output, "namespaces", ns_list);
	json_dumpf(output, f, JSON_SORT_KEYS | JSON_COMPACT);
	json_decref(output);
}

static struct frontend fe_json = {
	.format = "json",
	.desc = "JSON, see plotnetcfg-json(5)",
	.output = json_output,
};

void frontend_json_register(void)
{
	frontend_register(&fe_json);
}
