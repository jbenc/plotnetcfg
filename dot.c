#include <stdio.h>
#include "handler.h"
#include "if.h"
#include "label.h"
#include "netns.h"
#include "utils.h"
#include "dot.h"

static void output_label(struct label *list)
{
	struct label *ptr;

	for (ptr = list; ptr; ptr = ptr->next)
		printf("\\n%s", ptr->text);
}

static void output_addresses(struct if_addr_entry *list)
{
	struct if_addr_entry *ptr;

	for (ptr = list; ptr; ptr = ptr->next) {
		printf("\\n%s", ptr->addr.formatted);
		if (ptr->peer.formatted)
			printf(" peer %s", ptr->peer.formatted);
	}
}

static void output_ifaces_pass1(struct if_entry *list)
{
	struct if_entry *ptr;

	for (ptr = list; ptr; ptr = ptr->next) {
		printf("%s [label=\"%s", ifid(ptr), ptr->if_name);
		if (ptr->driver)
			printf(" (%s)", ptr->driver);
		output_label(ptr->label);
		output_addresses(ptr->addr);
		printf("\"");
		if (ptr->flags & IF_INTERNAL)
			printf(",style=dotted");
		else if (!(ptr->flags & IF_UP))
			printf(",style=filled,fillcolor=\"grey\"");
		else if (!(ptr->flags & IF_HAS_LINK))
			printf(",style=filled,fillcolor=\"pink\"");
		else
			printf(",style=filled,fillcolor=\"darkolivegreen1\"");
		printf("]\n");
	}
}

static void output_ifaces_pass2(struct if_entry *list)
{
	struct if_entry *ptr;

	for (ptr = list; ptr; ptr = ptr->next) {
		if (ptr->master) {
			printf("%s -> ", ifid(ptr));
			printf("%s", ifid(ptr->master));
			if (ptr->edge_label && !ptr->link)
				printf(" [label=\"%s\"]", ptr->edge_label);
			printf("\n");
		}
		if (ptr->link) {
			printf("%s -> ", ifid(ptr->link));
			printf("%s", ifid(ptr));
			if (ptr->edge_label)
				printf(" [label=\"%s\"]", ptr->edge_label);
			printf("\n");
		}
		if (ptr->peer &&
		    (unsigned long)ptr > (unsigned long)ptr->peer) {
			printf("%s -> ", ifid(ptr));
			printf("%s [dir=none,style=dotted]\n", ifid(ptr->peer));
		}
	}
}

void dot_output(struct netns_entry *root)
{
	struct netns_entry *ns;

	printf("digraph {\nnode [shape=box]\n");
	for (ns = root; ns; ns = ns->next) {
		if (ns->name)
			printf("subgraph cluster_%s {\nlabel=\"%s\"\n", ns->name, ns->name);
		output_ifaces_pass1(ns->ifaces);
		if (ns->name)
			printf("}");
	}
	for (ns = root; ns; ns = ns->next) {
		output_ifaces_pass2(ns->ifaces);
	}
	printf("}\n");
}
