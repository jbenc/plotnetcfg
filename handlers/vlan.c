#define _GNU_SOURCE
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <libnetlink.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "../handler.h"
#include "vlan.h"

static int vlan_netlink(struct if_entry *entry, struct rtattr **tb);

static struct handler h_vlan = {
	.driver = "802.1Q VLAN Support",
	.netlink = vlan_netlink,
	.cleanup = handler_generic_cleanup,
};

struct vlan_private {
	unsigned int tag;
};

void handler_vlan_register(void)
{
	handler_register(&h_vlan);
}

static int vlan_netlink(struct if_entry *entry, struct rtattr **tb)
{
	struct vlan_private *priv;
	struct rtattr *linkinfo[IFLA_INFO_MAX + 1];
	struct rtattr *vlanattr[IFLA_VLAN_MAX + 1];

	priv = calloc(sizeof(*priv), 1);
	if (!priv)
		return ENOMEM;
	entry->handler_private = priv;

	if (!tb[IFLA_LINKINFO])
		return ENOENT;
	parse_rtattr_nested(linkinfo, IFLA_INFO_MAX, tb[IFLA_LINKINFO]);
	if (!linkinfo[IFLA_INFO_DATA])
		return ENOENT;
	parse_rtattr_nested(vlanattr, IFLA_VLAN_MAX, linkinfo[IFLA_INFO_DATA]);
	if (!vlanattr[IFLA_VLAN_ID] ||
	    RTA_PAYLOAD(vlanattr[IFLA_VLAN_ID]) < sizeof(__u16))
		return ENOENT;
	priv->tag = rta_getattr_u16(vlanattr[IFLA_VLAN_ID]);
	if (asprintf(&entry->edge_label, "tag %d", priv->tag) < 0)
		return ENOMEM;
	return 0;
}
