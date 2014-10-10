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

#ifndef _HANDLER_H
#define _HANDLER_H
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <libnetlink.h>
#include "if.h"
#include "netns.h"

/* Only one handler for each driver allowed.
 * Generic handlers called for every interface are supported and are created
 * by setting driver to NULL. Generic handlers are not allowed to use
 * handler_private field in struct if_entry.
 *
 * The scan callback is called during interface scanning. Only actions
 * related to the passed single interface may be performed.
 * The post callback is called after all interfaces are scanned.
 * Inter-interface analysis is possible at this step.
 */
struct handler {
	struct handler *next;
	const char *driver;
	int (*netlink)(struct if_entry *entry, struct rtattr **tb);
	int (*scan)(struct if_entry *entry);
	int (*post)(struct if_entry *entry, struct netns_entry *root);
	void (*cleanup)(struct if_entry *entry);
};

void handler_register(struct handler *h);
int handler_netlink(struct if_entry *entry, struct rtattr **tb);
int handler_scan(struct if_entry *entry);
int handler_post(struct netns_entry *root);
void handler_cleanup(struct if_entry *entry);

/* For use as a callback. It calls free() on handler_private. */
void handler_generic_cleanup(struct if_entry *entry);

struct global_handler {
	struct global_handler *next;
	int (*post)(struct netns_entry *root);
	void (*cleanup)(struct netns_entry *root);
};

void global_handler_register(struct global_handler *h);
int global_handler_post(struct netns_entry *root);
void global_handler_cleanup(struct netns_entry *root);

/* callback returns 0 to ignore the interface, < 0 for error, > 0 for
 * priority.
 * The highest priority match is returned if exactly one highest priority
 * interface matches. Returns -1 if more highest priority interfaces match.
 * Returns 0 for success (*found will be NULL for no match) or error
 * code > 0.
 */
int find_interface(struct if_entry **found,
		   struct netns_entry *root, int all_ns,
		   struct if_entry *self,
		   int (*callback)(struct if_entry *, void *),
		   void *arg);

#endif
