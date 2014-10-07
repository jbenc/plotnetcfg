#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../ethtool.h"
#include "../handler.h"
#include "../utils.h"
#include "veth.h"

static int veth_scan(struct if_entry *entry);
static int veth_post(struct if_entry *entry, struct netns_entry *root);

static struct handler h_veth = {
	.driver = "veth",
	.scan = veth_scan,
	.post = veth_post,
};

void handler_veth_register(void)
{
	handler_register(&h_veth);
}

static int veth_scan(struct if_entry *entry)
{
	entry->peer_index = ethtool_veth_peer(entry->if_name);
	return 0;
}

static int match_peer(struct if_entry *entry, void *arg)
{
	struct if_entry *link = arg;

	if (entry->if_index != link->peer_index ||
	    entry->peer_index != link->if_index ||
	    strcmp(entry->driver, "veth"))
		return 0;
	if (entry->peer && entry->peer != link)
		return 0;
	if (entry->ns == link->ns)
		return 2;
	return 1;
}

static int veth_post(struct if_entry *entry, struct netns_entry *root)
{
	int err;

	if (!entry->peer_index)
		return ENOENT;
	err = find_interface(&entry->peer, root, 1, entry, match_peer, entry);
	if (err > 0)
		return err;
	if (err < 0) {
		fprintf(stderr, "ERROR: cannot find the veth peer for %s reliably.\n",
			ifstr(entry));
		return EEXIST;
	}
	if (!entry->peer) {
		fprintf(stderr, "ERROR: cannot find the veth peer for %s.\n",
			ifstr(entry));
		return ENOENT;
	}
	return 0;
}
