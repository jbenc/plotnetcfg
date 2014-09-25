#ifndef _HANDLER_H
#define _HANDLER_H
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
	int (*scan)(struct if_entry *entry);
	int (*post)(struct if_entry *entry, struct netns_entry *root);
	int (*cleanup)(struct if_entry *entry);
};

int handler_scan(struct if_entry *entry);
int handler_post(struct netns_entry *root);
void handler_cleanup(struct if_entry *entry);

#endif
