#ifndef _NETNS_H
#define _NETNS_H
#include "if.h"

struct netns_entry {
	struct netns_entry *next;
	struct if_entry *ifaces;
	char *name;
};

int netns_list(struct netns_entry **result);
void netns_list_free(struct netns_entry *list);
int netns_switch(struct netns_entry *dest);

#endif
