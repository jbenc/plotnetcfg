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
	int err;

	handler_err_loop(err, netlink, entry, tb);
	return err;
}

int handler_scan(struct if_entry *entry)
{
	int err;

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

#define ghandler_loop(method, root, ...)				\
	{								\
		struct global_handler *ptr;				\
									\
		for (ptr = ghandlers; ptr; ptr = ptr->next) {		\
			if (!ptr->method)	\
				continue;				\
			ptr->method(root, ##__VA_ARGS__);		\
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

int find_interface(struct if_entry **found,
		   struct netns_entry *root, int all_ns,
		   struct if_entry *self,
		   int (*callback)(struct if_entry *, void *),
		   void *arg)
{
	struct netns_entry *ns;
	struct if_entry *entry;
	int prio = 0, count = 0, res;

	for (ns = root; ns; ns = ns->next) {
		for (entry = ns->ifaces; entry; entry = entry->next) {
			if (entry == self)
				continue;
			res = callback(entry, arg);
			if (res < 0)
				return -res;
			if (res > prio) {
				*found = entry;
				prio = res;
				count = 1;
			} else if (res == prio)
				count++;
		}
		if (!all_ns)
			break;
	}
	if (!prio) {
		*found = NULL;
		return 0;
	}
	if (count > 1)
		return -1;
	return 0;
}
