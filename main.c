#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "if.h"
#include "netns.h"
#include "utils.h"

static void error(char *msg, int err)
{
	fprintf(stderr, "ERROR: %s: %s\n", msg, strerror(err));
	exit(1);
}

static void dump_addresses(struct if_addr_entry *list)
{
	struct if_addr_entry *ptr;

	for (ptr = list; ptr; ptr = ptr->next) {
		printf("    %s", ptr->addr);
		if (ptr->peer)
			printf(" peer %s", ptr->peer);
		printf("\n");
	}
}

static void dump_ifaces(struct if_entry *list)
{
	struct if_entry *ptr;

	for (ptr = list; ptr; ptr = ptr->next) {
		printf("  %d: %s\n", ptr->if_index, ptr->if_name);
		if (ptr->driver)
			printf("    driver: %s\n", ptr->driver);
		dump_addresses(ptr->addr);
	}
}

int main(_unused int argc, _unused char **argv)
{
	struct netns_entry *nslist, *nsptr;
	int err;

	if ((err = netns_list(&nslist)))
		error("get netns list", err);

	for (nsptr = nslist; nsptr; nsptr = nsptr->next) {
		if (nsptr->name)
			printf("Namespace: %s\n", nsptr->name);
		dump_ifaces(nsptr->ifaces);
	}

	netns_list_free(nslist);
	return 0;
}
