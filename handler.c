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

#include <stdlib.h>
#include <string.h>
#include "netns.h"
#include "if.h"
#include "handler.h"

static struct handler *handlers = NULL;
static struct handler *handlers_tail = NULL;

static struct global_handler *ghandlers = NULL;
static struct global_handler *ghandlers_tail = NULL;

void handler_register(struct handler *h)
{
	h->next = NULL;
	if (!handlers) {
		handlers = handlers_tail = h;
		return;
	}
	handlers_tail->next = h;
	handlers_tail = h;
}

static int driver_match(struct handler *h, struct if_entry *e)
{
	return !h->driver || (e->driver && !strcmp(h->driver, e->driver));
}

#define handler_err_loop(err, method, entry, ...)				\
	{								\
		struct handler *ptr;					\
									\
		for (ptr = handlers; ptr; ptr = ptr->next) {		\
			if (!ptr->method || !driver_match(ptr, entry))	\
				continue;				\
			err = ptr->method(entry, ##__VA_ARGS__);	\
			if (err)					\
				break;					\
		}							\
	}

#define handler_loop(method, entry, ...)				\
	{								\
		struct handler *ptr;					\
									\
		for (ptr = handlers; ptr; ptr = ptr->next) {		\
			if (!ptr->method || !driver_match(ptr, entry))	\
				continue;				\
			ptr->method(entry, ##__VA_ARGS__);		\
		}							\
	}

int handler_netlink(struct if_entry *entry, struct rtattr **tb)
{
	int err = 0;

	handler_err_loop(err, netlink, entry, tb);
	return err;
}

int handler_scan(struct if_entry *entry)
{
	int err = 0;

	handler_err_loop(err, scan, entry);
	return err;
}

int handler_post(struct netns_entry *root)
{
	struct netns_entry *ns;
	struct if_entry *entry;
	int err;

	for (ns = root; ns; ns = ns->next) {
		for (entry = ns->ifaces; entry; entry = entry->next) {
			handler_err_loop(err, post, entry, root);
			if (err)
				return err;
		}
	}
	return 0;
}

void handler_cleanup(struct if_entry *entry)
{
	handler_loop(cleanup, entry);
}

void handler_generic_cleanup(struct if_entry *entry)
{
	free(entry->handler_private);
}

#define ghandler_loop(method, ...)				\
	{								\
		struct global_handler *ptr;				\
									\
		for (ptr = ghandlers; ptr; ptr = ptr->next) {		\
			if (!ptr->method)	\
				continue;				\
			ptr->method(__VA_ARGS__);			\
		}							\
	}

void global_handler_register(struct global_handler *h)
{
	h->next = NULL;
	if (!ghandlers) {
		ghandlers = ghandlers_tail = h;
		return;
	}
	ghandlers_tail->next = h;
	ghandlers_tail = h;
}

void global_handler_init(void)
{
	ghandler_loop(init);
}

int global_handler_post(struct netns_entry *root)
{
	struct global_handler *h;
	int err;

	for (h = ghandlers; h; h = h->next) {
		if (!h->post)
			continue;
		err = h->post(root);
		if (err)
			return err;
	}
	return 0;
}

void global_handler_cleanup(struct netns_entry *root)
{
	ghandler_loop(cleanup, root);
}
