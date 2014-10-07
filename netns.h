#ifndef _NETNS_H
#define _NETNS_H

struct if_entry;

struct netns_entry {
	struct netns_entry *next;
	struct if_entry *ifaces;
	char *name;
};

int netns_list(struct netns_entry **result, int supported);
void netns_list_free(struct netns_entry *list);
int netns_switch(struct netns_entry *dest);
int netns_switch_root(void);

#endif
