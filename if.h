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

#ifndef _IF_H
#define _IF_H

struct netns_entry;
struct label;

struct addr {
	int family;
	int prefixlen;
	char *formatted;
	void *raw;
};

struct if_addr_entry {
	struct if_addr_entry *next;
	struct addr addr;
	struct addr peer;
};

struct if_list_entry {
	struct if_list_entry *next;
	struct if_entry *entry;
};

struct if_entry {
	struct if_entry *next;
	struct netns_entry *ns;
	char *internal_ns;
	unsigned int if_index;
	unsigned int flags;
	int mtu;
	char *if_name;
	char *driver;
	struct label *label;
	unsigned int master_index;
	struct if_entry *master;
	unsigned int link_index;
	int link_netnsid;
	struct if_entry *link;
	unsigned int peer_index;
	int peer_netnsid;
	struct if_entry *peer;
	struct if_addr_entry *addr;
	char *edge_label;
	void *handler_private;
	int warnings;
	/* reverse fields needed by some frontends: */
	struct if_list_entry *rev_master;
	struct if_entry *rev_link;
	char *pci_path;
	char *pci_physfn_path;
	struct if_entry *physfn;
};

#define IF_LOOPBACK	1
#define IF_UP		2
#define IF_HAS_LINK	4
#define IF_INTERNAL	8
#define IF_PEER_WEAK	16

int if_list(struct if_entry **result, struct netns_entry *ns);
void if_list_free(struct if_entry *list);

void if_append(struct if_entry **list, struct if_entry *item);

int if_add_warning(struct if_entry *entry, char *fmt, ...);
int if_list_build_rev(struct if_entry *list);

#endif
