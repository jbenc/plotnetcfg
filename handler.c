#include <stdlib.h>
#include <string.h>
#include "netns.h"
#include "if.h"
#include "handler.h"

static struct handler *handlers = NULL;

void handler_register(struct handler *h)
{
	h->next = handlers;
	handlers = h;
}

static int driver_match(struct handler *h, struct if_entry *e)
{
	return !h->driver || !strcmp(h->driver, e->driver);
}

#define handler_loop(err, method, entry, ...)				\
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

int handler_scan(struct if_entry *entry)
{
	int err;

	handler_loop(err, scan, entry);
	return err;
}

int handler_post(struct netns_entry *root)
{
	struct netns_entry *ns;
	struct if_entry *entry;
	int err;

	for (ns = root; ns; ns = ns->next) {
		for (entry = ns->ifaces; entry; entry = entry->next) {
			handler_loop(err, post, entry, root);
			if (err)
				return err;
		}
	}
	return 0;
}

void handler_cleanup(struct if_entry *entry)
{
	int err;

	handler_loop(err, cleanup, entry);
}

int find_interface(struct if_entry **found,
		   struct netns_entry *root, struct if_entry *self,
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
	}
	if (!prio) {
		*found = NULL;
		return 0;
	}
	if (count > 1)
		return -1;
	return 0;
}
