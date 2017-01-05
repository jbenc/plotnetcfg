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

#include "handler.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "if.h"
#include "netns.h"

struct handler_list {
	struct handler *head, *tail;
};

static struct handler_list if_handlers, netns_handlers, global_handlers;

void handler_register(struct handler_list *list, struct handler *h)
{
	h->next = NULL;
	if (!list->head)
		list->head = h;
	else
		list->tail->next = h;
	list->tail = h;
}

void if_handler_register(struct if_handler *h)
{
	handler_register(&if_handlers, (struct handler *) h);
}

void netns_handler_register(struct netns_handler *h)
{
	handler_register(&netns_handlers, (struct handler *) h);
}

void global_handler_register(struct global_handler *h)
{
	handler_register(&global_handlers, (struct handler *) h);
}

static int driver_match(struct if_handler *h, struct if_entry *e)
{
	return !h->driver || (e->driver && !strcmp(h->driver, e->driver));
}

#define for_each_handler(ptr, list) \
    for (ptr = (void *) (list).head; ptr; ptr = (void *) ptr->handler.next)

#define handler_callback(handler, callback, ...)				\
	((handler)->callback ? (handler)->callback(__VA_ARGS__) : 0)

#define if_handler_callback(handler, callback, entry, ...)				\
	(driver_match(handler, entry) ? handler_callback(handler, callback, entry, ##__VA_ARGS__) : 0)

int if_handler_init(struct if_entry *entry)
{
	struct if_handler *h;

	for_each_handler(h, if_handlers) {
		if (!h->driver || strcmp(h->driver, entry->driver))
			continue;

		if (h->private_size) {
			entry->handler_private = calloc(1, h->private_size);
			if (!entry->handler_private)
				return ENOMEM;
		}

		break;
	}

	return 0;
}

int if_handler_netlink(struct if_entry *entry, struct rtattr **linkinfo)
{
	struct if_handler *h;
	int err;

	for_each_handler(h, if_handlers)
		if ((err = if_handler_callback(h, netlink, entry, linkinfo)))
			return err;

	return 0;
}

int if_handler_scan(struct if_entry *entry)
{
	struct if_handler *h;
	int err;

	for_each_handler(h, if_handlers)
		if ((err = if_handler_callback(h, scan, entry)))
			return err;

	return 0;
}

int if_handler_post(struct netns_entry *root)
{
	struct netns_entry *ns;
	struct if_entry *entry;
	struct if_handler *h;
	int err;

	for (ns = root; ns; ns = ns->next)
		list_for_each(entry, root->ifaces)
			for_each_handler(h, if_handlers)
				if ((err = if_handler_callback(h, post, entry, root)))
					return err;

	return 0;
}

void if_handler_cleanup(struct if_entry *entry)
{
	struct if_handler *h;

	for_each_handler(h, if_handlers)
		if_handler_callback(h, cleanup, entry);

	if (entry->handler_private)
		free(entry->handler_private);
}

int netns_handler_scan(struct netns_entry *entry)
{
	struct netns_handler *h;
	int err;

	for_each_handler(h, netns_handlers)
		if ((err = handler_callback(h, scan, entry)))
			return err;

	return 0;
}

void netns_handler_cleanup(struct netns_entry *entry)
{
	struct netns_handler *h;

	for_each_handler(h, netns_handlers)
		handler_callback(h, cleanup, entry);
}

int global_handler_init(void)
{
	struct global_handler *h;
	int err;

	for_each_handler(h, global_handlers)
		if ((err = handler_callback(h, init)))
			return err;

	return 0;
}

int global_handler_post(struct netns_entry *root)
{
	struct global_handler *h;
	int err;

	for_each_handler(h, global_handlers)
		if ((err = handler_callback(h, post, root)))
			return err;

	return 0;
}

void global_handler_cleanup(struct netns_entry *root)
{
	struct global_handler *h;

	for_each_handler(h, global_handlers)
		handler_callback(h, cleanup, root);
}
