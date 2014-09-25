#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dot.h"
#include "netns.h"
#include "utils.h"
#include "handlers/master.h"
#include "handlers/veth.h"

static void register_handlers(void)
{
	handler_master_register();
	handler_veth_register();
}

int main(_unused int argc, _unused char **argv)
{
	struct netns_entry *root;
	int err;

	register_handlers();

	if ((err = netns_list(&root))) {
		fprintf(stderr, "ERROR: %s\n", strerror(err));
		exit(1);
	}
	dot_output(root);
	netns_list_free(root);

	return 0;
}
