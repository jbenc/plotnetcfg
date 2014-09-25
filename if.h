#ifndef _IF_H
#define _IF_H

struct if_addr_entry {
	struct if_addr_entry *next;
	int family;
	char *addr;
	char *peer;
};

struct if_entry {
	struct if_entry *next;
	unsigned int if_index;
	char *if_name;
	char *driver;
	unsigned int master_index;
	struct if_addr_entry *addr;
	void *handler_private;
};

int if_list(struct if_entry **result);
void if_list_free(struct if_entry *list);

#endif
