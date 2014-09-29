#ifndef _IF_H
#define _IF_H

struct netns_entry;
struct label;

struct if_addr_entry {
	struct if_addr_entry *next;
	int family;
	char *addr;
	char *peer;
};

struct if_entry {
	struct if_entry *next;
	struct netns_entry *ns;
	unsigned int if_index;
	unsigned int if_flags;
	char *if_name;
	char *driver;
	struct label *label;
	unsigned int master_index;
	struct if_entry *master;
	unsigned int link_index;
	struct if_entry *link;
	unsigned int peer_index;
	struct if_entry *peer;
	struct if_addr_entry *addr;
	char *edge_label;
	void *handler_private;
};

int if_list(struct if_entry **result, struct netns_entry *ns);
void if_list_free(struct if_entry *list);

#endif
