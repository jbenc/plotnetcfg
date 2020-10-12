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

#include "dot.h"
#include <stdio.h>
#include <sys/socket.h>
#include <time.h>
#include "../addr.h"
#include "../frontend.h"
#include "../handler.h"
#include "../if.h"
#include "../netns.h"
#include "../utils.h"
#include "../version.h"

static void output_label(FILE *f, struct list *labels)
{
	struct label *ptr;

	list_for_each(ptr, *labels)
		fprintf(f, "\\n%s", ptr->text);
}

static void output_label_properties(FILE *f, struct list *properties, unsigned int prop_mask)
{
	struct label_property *ptr;

	list_for_each(ptr, *properties)
		if (label_prop_match_mask(ptr->type, prop_mask))
			fprintf(f, "\\n%s: %s", ptr->key, ptr->value);
}

static void output_addresses(FILE *f, struct list *addresses)
{
	struct if_addr *addr;

	list_for_each(addr, *addresses) {
		fprintf(f, "\\n%s", addr->addr.formatted);
		if (addr->peer.formatted)
			fprintf(f, " peer %s", addr->peer.formatted);
	}
}

static void output_mtu(FILE *f, struct if_entry *ptr)
{
	if (ptr->mtu && ptr->mtu != 1500 && !(ptr->flags & IF_LOOPBACK))
		fprintf(f, "\\nMTU %d", ptr->mtu);
}

static void output_xdp(FILE *f, struct list *xdp_list)
{
	struct if_xdp *xdp;

	list_for_each(xdp, *xdp_list)
		fprintf(f, "\\nxdp-%s (id %u)", xdp->mode, xdp->prog_id);
}

static int output_ifaces_pass1(FILE *f, struct list *list, unsigned int prop_mask)
{
	int need_init_net = 0;
	struct if_entry *ptr;

	list_for_each(ptr, *list) {
		fprintf(f, "\"%s\" [label=\"%s", ifid(ptr), ptr->if_name);
		if (ptr->driver)
			fprintf(f, " (%s)", ifdrv(ptr));
		output_label_properties(f, &ptr->properties, prop_mask);
		output_mtu(f, ptr);
		output_xdp(f, &ptr->xdp);
		if (label_prop_match_mask(IF_PROP_CONFIG, prop_mask)) {
			output_addresses(f, &ptr->addr);
			if ((ptr->flags & IF_LOOPBACK) == 0 && ptr->mac_addr.formatted)
				fprintf(f, "\\nmac %s", ptr->mac_addr.formatted);
		}
		fprintf(f, "\"");

		if (label_prop_match_mask(IF_PROP_STATE, prop_mask)) {
			if (ptr->flags & IF_INTERNAL)
				fprintf(f, ",style=dotted");
			else if (!(ptr->flags & IF_UP))
				fprintf(f, ",style=filled,fillcolor=\"grey\"");
			else if (!(ptr->flags & IF_HAS_LINK))
				fprintf(f, ",style=filled,fillcolor=\"pink\"");
			else
				fprintf(f, ",style=filled,fillcolor=\"darkolivegreen1\"");
		}
		if (ptr->warnings)
			fprintf(f, ",color=\"red\"");
		fprintf(f, "]\n");

		if (ptr->link_net && !ptr->link_net->name)
			need_init_net = 1;
	}

	return need_init_net;
}

static void output_ifaces_pass2(FILE *f, struct list *list)
{
	struct if_entry *ptr;

	list_for_each(ptr, *list) {
		if (ptr->master) {
			fprintf(f, "\"%s\" -> ", ifid(ptr));
			fprintf(f, "\"%s\" [style=%s", ifid(ptr->master),
			       ptr->flags & IF_PASSIVE_SLAVE ? "dashed" : "solid");
			if (ptr->edge_label && !ptr->link)
				fprintf(f, ",label=\"%s\"", ptr->edge_label);
			fprintf(f, "]\n");
		}
		if (ptr->physfn) {
			fprintf(f, "\"%s\" -> ", ifid(ptr));
			fprintf(f, "\"%s\" [style=dotted,taillabel=\"PF\"]\n", ifid(ptr->physfn));
		}
		if (ptr->link) {
			fprintf(f, "\"%s\" -> ", ifid(ptr->link));
			fprintf(f, "\"%s\" [style=%s", ifid(ptr),
				ptr->flags & IF_LINK_WEAK ? "dashed" : "solid");
			if (ptr->edge_label)
				fprintf(f, ",label=\"%s\"", ptr->edge_label);
			fprintf(f, "]\n");
		} else if (ptr->link_net) {
			if (ptr->link_net->name) {
				/* hack: we can't create an arrow directly
				 * to the cluster, only to a node. lhead
				 * modifies the arrow so that it actually
				 * points to the cluster.
				 */
				fprintf(f, "\"%s\" -> ", ifid(ptr));
				fprintf(f, "\"%s/lo\" ", nsid(ptr->link_net));
				fprintf(f, "[lhead=\"cluster/%s\", style=\"dashed\"]\n", nsid(ptr->link_net));
			} else
				fprintf(f, "\"%s\" -> \"cluster//\" [style=\"dashed\"]\n", ifid(ptr));
		}
		if (ptr->peer && (size_t) ptr > (size_t) ptr->peer) {
			fprintf(f, "\"%s\" -> ", ifid(ptr));
			fprintf(f, "\"%s\" [dir=none]\n", ifid(ptr->peer));
		}
	}
}

static void output_warnings(FILE *f, struct list *netns_list)
{
	struct netns_entry *ns;
	int was_label = 0;

	list_for_each(ns, *netns_list) {
		if (!list_empty(ns->warnings)) {
			if (!was_label)
				fprintf(f, "label=\"");
			was_label = 1;
			output_label(f, &ns->warnings);
		}
	}
	if (was_label) {
		fprintf(f, "\"\n");
		fprintf(f, "fontcolor=\"red\"\n");
	}
}

static void dot_output(FILE *f, struct list *netns_list, struct output_entry *output_entry)
{
	struct netns_entry *ns;
	int need_init_net = 0;
	time_t cur;

	time(&cur);
	fprintf(f, "// generated by plotnetcfg " VERSION " on %s", ctime(&cur));
	fprintf(f, "digraph {\ncompound=true;\nnode [shape=box]\n");
	list_for_each(ns, *netns_list) {
		if (ns->name) {
			fprintf(f, "subgraph \"cluster/%s\" {\n", nsid(ns));
			fprintf(f, "label=\"%s\"\n", ns->name);
			fprintf(f, "fontcolor=\"black\"\n");
		}
		need_init_net += output_ifaces_pass1(f, &ns->ifaces, output_entry->print_mask);
		if (ns->name)
			fprintf(f, "}\n");
	}

	/* output a fake cluster to which tunnel interfaces can point */
	if (need_init_net)
		fprintf(f, "\"cluster//\"[label=\"root netns\",penwidth=0];\n");

	list_for_each(ns, *netns_list)
		output_ifaces_pass2(f, &ns->ifaces);
	output_warnings(f, netns_list);
	fprintf(f, "}\n");
}

static struct frontend fe_dot = {
	.format = "dot",
	.desc = "dot language (suitable for graphviz)",
	.output = dot_output,
};

void frontend_dot_register(void)
{
	frontend_register(&fe_dot);
}
