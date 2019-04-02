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

#include <errno.h>
#include <linux/capability.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h>
#include "args.h"
#include "netns.h"
#include "utils.h"
#include "version.h"

#include "frontend.h"
#include "frontends/dot.h"
#include "frontends/json.h"

#include "handler.h"
#include "handlers/bond.h"
#include "handlers/bridge.h"
#include "handlers/gre.h"
#include "handlers/iov.h"
#include "handlers/openvswitch.h"
#include "handlers/team.h"
#include "handlers/veth.h"
#include "handlers/vlan.h"
#include "handlers/vxlan.h"
#include "handlers/route.h"

static void register_frontends(void)
{
	frontend_init();
	frontend_dot_register();
	frontend_json_register();
}

static void register_handlers(void)
{
	handler_bond_register();
	handler_bridge_register();
	handler_gre_register();
	handler_iov_register();
	handler_openvswitch_register();
	handler_team_register();
	handler_veth_register();
	handler_vlan_register();
	handler_vxlan_register();
	handler_route_register();
}

static int print_help(_unused char *arg)
{
	printf("Usage: plotnetcfg [OPTION]...\n\n");
	arg_get_help((arg_help_handler_t)puts);
	return 1;
}

static int print_version(_unused char *arg)
{
	printf("%s\n", VERSION);
	return 1;
}

static struct arg_option options[] = {
	{ .long_name = "help", .short_name = 'h',
	  .type = ARG_CALLBACK, .action.callback = print_help,
	  .help = "print help and exit",
	},
	{ .long_name = "version", .short_name = '\0',
	  .type = ARG_CALLBACK, .action.callback = print_version,
	  .help = "print version and exit",
	},
};

static int check_caps(void)
{
	struct __user_cap_header_struct caps_hdr;
	struct __user_cap_data_struct caps[2];
	int err;

	caps_hdr.version = _LINUX_CAPABILITY_VERSION_3;
	caps_hdr.pid = 0;
	err = syscall(__NR_capget, &caps_hdr, caps);
	if (err == -EINVAL) {
		caps_hdr.version = _LINUX_CAPABILITY_VERSION_1;
		caps_hdr.pid = 0;
		err = syscall(__NR_capget, &caps_hdr, caps);
	}
	if (err < 0)
		return 0;
	/* The capabilities we're interested in fall into the first 32 bits,
	 * thus we can just check the first array member irrespective of
	 * whether we used v3 or v1 of the syscall. */
	if (!(caps[0].effective & (1U << CAP_SYS_ADMIN)) ||
	    !(caps[0].effective & (1U << CAP_NET_ADMIN)))
		return 0;
	return 1;
}

int main(int argc, char **argv)
{
	struct list netns_list;
	int netns_ok, err;

	arg_register_batch(options, ARRAY_SIZE(options));
	register_frontends();
	register_handlers();
	if ((err = arg_parse(argc, argv)))
		exit(err);

	if (!check_caps()) {
		fprintf(stderr, "Must be run under root (or with enough capabilities).\n");
		exit(1);
	}
	netns_ok = netns_switch_root();
	if (netns_ok > 0) {
		fprintf(stderr, "Cannot change to the root name space: %s\n", strerror(netns_ok));
		exit(1);
	}

	if ((err = global_handler_init())) {
		fprintf(stderr, "Initialization failed: %s\n", strerror(err));
		exit(1);
	}
	if ((err = netns_fill_list(&netns_list, netns_ok == 0))) {
		fprintf(stderr, "ERROR: %s\n", strerror(err));
		exit(1);
	}
	if ((err = frontend_output(&netns_list))) {
		fprintf(stderr, "Invalid output format specified.\n");
		exit(1);
	}
	global_handler_cleanup(&netns_list);
	netns_list_free(&netns_list);
	frontend_cleanup();

	return 0;
}
