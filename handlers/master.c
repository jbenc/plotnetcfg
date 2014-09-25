/* Resolves master_index to the actual interface. */

#include <errno.h>
#include <stdio.h>
#include "../handler.h"
#include "../utils.h"
#include "master.h"

static int master_post(struct if_entry *entry, struct netns_entry *root);

static struct handler h_master = {
	.post = master_post,
};

void handler_master_register(void)
{
	handler_register(&h_master);
}

static int match_master(struct if_entry *entry, void *arg)
{
	struct if_entry *slave = arg;

	if (entry->if_index != slave->master_index)
		return 0;
	if (entry->ns == slave->ns)
		return 2;
	return 1;
}

static int master_post(struct if_entry *entry, struct netns_entry *root)
{
	int err;

	if (!entry->master_index)
		return 0;
	err = find_interface(&entry->master, root, entry, match_master, entry);
	if (err > 0)
		return err;
	if (err < 0) {
		fprintf(stderr, "ERROR: cannot find master for %s reliably.\n",
			ifstr(entry));
		return EEXIST;
	}
	if (!entry->master) {
		fprintf(stderr, "ERROR: cannot find master for %s.\n",
			ifstr(entry));
		return ENOENT;
	}
	return 0;
}
