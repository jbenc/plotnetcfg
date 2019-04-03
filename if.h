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

#include "addr.h"
#include "label.h"
#include "list.h"

struct netns_entry;

struct if_addr {
	struct node n;
	struct addr addr;
	struct addr peer;
};

struct if_entry {
	struct node n;			/* in netns->ifaces        */
	struct node rev_master_node;	/* in if_entry->rev_master */
	struct node rev_link_node;	/* in if_entry->rev_link   */

	/* basic fields */
	struct netns_entry *ns;
	char *internal_ns;
	unsigned int if_index;
	unsigned int flags;
	int mtu;
	char *if_name;
	char *driver;
	char *sub_driver;
	struct list properties;
	struct mac_addr mac_addr;
	struct list addr;

	/* master relation */
	unsigned int master_index;
	struct if_entry *master;

	/* peer and child relations */
	struct if_entry *active_slave;
	unsigned int link_index;
	int link_netnsid;
	struct if_entry *link;
	unsigned int peer_index;
	int peer_netnsid;
	struct if_entry *peer;

	/* IOV fields */
	char *pci_path;
	char *pci_physfn_path;
	struct if_entry *physfn;

	/* handler fields */
	char *edge_label;
	void *handler_private;
	int warnings;

	/* reverse fields needed by some frontends */
	struct list rev_master;
	struct list rev_link;
};

#define IF_LOOPBACK		1
#define IF_UP			2
#define IF_HAS_LINK		4
#define IF_INTERNAL		8
#define IF_LINK_WEAK		16
#define IF_PASSIVE_SLAVE	32

int if_list(struct list *result, struct netns_entry *ns);
void if_list_free(struct list *list);
struct if_entry *if_create(void);

int if_add_warning(struct if_entry *entry, char *fmt, ...);

#define IF_PROP_STATE	1
#define IF_PROP_CONFIG	2

#define if_add_state(entry, key, fmt, ...) label_add_property(&(entry)->properties, IF_PROP_STATE, key, fmt, ##__VA_ARGS__)
#define if_add_config(entry, key, fmt, ...) label_add_property(&(entry)->properties, IF_PROP_CONFIG, key, fmt, ##__VA_ARGS__)

#endif
