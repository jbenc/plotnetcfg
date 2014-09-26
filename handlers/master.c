/* Resolves master_index to the actual interface. */

#include <errno.h>
#include <stdio.h>
#include "../handler.h"
#include "../utils.h"
#include "master.h"

static int master_post(struct netns_entry *root);

static struct global_handler h_master = {
	.post = master_post,
};

void handler_master_register(void)
{
	global_handler_register(&h_master);
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

static int process(struct if_entry *entry, struct netns_entry *root)
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

static int master_post(struct netns_entry *root)
{
	struct netns_entry *ns;
	struct if_entry *entry;
	int err;

	for (ns = root; ns; ns = ns->next) {
		for (entry = ns->ifaces; entry; entry = entry->next) {
			err = process(entry, root);
			if (err)
				return err;
		}
	}
	return 0;
}
