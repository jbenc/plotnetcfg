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

#ifndef _NETNS_H
#define _NETNS_H

#include <sys/types.h>

struct if_entry;
struct label;
struct netns_entry;
struct route;

struct netns_id {
	struct netns_id *next;
	struct netns_entry *ns;
	int id;
};

struct netns_entry {
	struct netns_entry *next;
	struct if_entry *ifaces;
	struct label *warnings;
	long kernel_id;
	/* name is NULL for root name space, for other name spaces it
	 * contains human recognizable identifier */
	char *name;
	pid_t pid;
	int fd;
	struct netns_id *ids;
	struct rtable *rtables;
};

int netns_list(struct netns_entry **result, int supported);
void netns_list_free(struct netns_entry *list);
int netns_switch(struct netns_entry *dest);
int netns_switch_root(void);

#endif
