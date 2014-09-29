#ifndef _IF_H
#define _IF_H

struct netns_entry;
struct label;

struct addr {
	int family;
	int prefixlen;
	char *formatted;
	void *raw;
};

struct if_addr_entry {
	struct if_addr_entry *next;
	struct addr addr;
	struct addr peer;
};

struct if_entry {
	struct if_entry *next;
	struct netns_entry *ns;
	char *internal_ns;
	unsigned int if_index;
	unsigned int flags;
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

#define IF_UP		1
#define IF_HAS_LINK	2
#define IF_INTERNAL	4

int if_list(struct if_entry **result, struct netns_entry *ns);
void if_list_free(struct if_entry *list);

void if_append(struct if_entry **list, struct if_entry *item);

#endif
