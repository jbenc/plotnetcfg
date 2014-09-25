#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "../ethtool.h"
#include "../handler.h"
#include "../utils.h"
#include "veth.h"

static int veth_scan(struct if_entry *entry);
static int veth_post(struct if_entry *entry, struct netns_entry *root);
static void veth_print(struct if_entry *entry);

static struct handler h_veth = {
	.driver = "veth",
	.scan = veth_scan,
	.post = veth_post,
	.print = veth_print,
	.cleanup = handler_generic_cleanup,
};

struct veth_private {
	unsigned int pair_index;
	struct if_entry *pair;
};

void handler_veth_register(void)
{
	handler_register(&h_veth);
}

static int veth_scan(struct if_entry *entry)
{
	struct veth_private *priv;

	priv = calloc(sizeof(*priv), 1);
	if (!priv)
		return ENOMEM;
	entry->handler_private = priv;

	priv->pair_index = ethtool_veth_pair(entry->if_name);
	return 0;
}

static int match_pair(struct if_entry *entry, void *arg)
{
	struct if_entry *link = arg;
	struct veth_private *link_priv = link->handler_private;
	struct veth_private *entry_priv = entry->handler_private;

	if (entry->if_index != link_priv->pair_index ||
	    entry_priv->pair_index != link->if_index)
		return 0;
	if (entry_priv->pair && entry_priv->pair != link)
		return 0;
	if (entry->ns == link->ns)
		return 2;
	return 1;
}

static int veth_post(struct if_entry *entry, struct netns_entry *root)
{
	struct veth_private *priv = entry->handler_private;
	int err;

	if (!priv->pair_index)
		return ENOENT;
	err = find_interface(&priv->pair, root, entry, match_pair, entry);
	if (err > 0)
		return err;
	if (err < 0) {
		fprintf(stderr, "ERROR: cannot find the other side for veth interface %s reliably.\n",
			ifstr(entry));
		return EEXIST;
	}
	if (!priv->pair) {
		fprintf(stderr, "ERROR: cannot find the other side for veth interface %s.\n",
			ifstr(entry));
		return ENOENT;
	}
	return 0;
}

static void veth_print(struct if_entry *entry)
{
	struct veth_private *priv = entry->handler_private;

	printf("    veth pair: %s\n", ifstr(priv->pair));
}
