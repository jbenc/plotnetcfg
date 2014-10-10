/*
 * This file is a part of plotnetcfg, a tool to visualize network config.
 * Copyright (C) 2014 Red Hat, Inc. -- Jiri Benc <jbenc@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/capability.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "dot.h"
#include "netns.h"
#include "utils.h"
#include "handler.h"
#include "handlers/bridge.h"
#include "handlers/master.h"
#include "handlers/openvswitch.h"
#include "handlers/veth.h"
#include "handlers/vlan.h"

static void register_handlers(void)
{
	handler_master_register();
	handler_ovs_register();
	handler_veth_register();
	handler_vlan_register();
	handler_bridge_register();
}

static int check_caps(void)
{
	struct __user_cap_header_struct caps_hdr;
	struct __user_cap_data_struct caps;

	caps_hdr.version = _LINUX_CAPABILITY_VERSION_1;
	caps_hdr.pid = 0;
	if (syscall(__NR_capget, &caps_hdr, &caps) < 0)
		return 0;
	if (!(caps.effective & (1U << CAP_SYS_ADMIN)) ||
	    !(caps.effective & (1U << CAP_NET_ADMIN)))
		return 0;
	return 1;
}

int main(_unused int argc, _unused char **argv)
{
	struct netns_entry *root;
	int netns_ok, err;

	if (!check_caps()) {
		fprintf(stderr, "Must be run under root (or with enough capabilities).\n");
		exit(1);
	}
	netns_ok = netns_switch_root();
	if (netns_ok > 0) {
		fprintf(stderr, "Cannot change to the root name space: %s\n", strerror(netns_ok));
		exit(1);
	}

	register_handlers();

	if ((err = netns_list(&root, netns_ok == 0))) {
		fprintf(stderr, "ERROR: %s\n", strerror(err));
		exit(1);
	}
	dot_output(root);
	global_handler_cleanup(root);
	netns_list_free(root);

	return 0;
}
