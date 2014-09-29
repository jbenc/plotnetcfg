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
	int err;

	if (!check_caps()) {
		fprintf(stderr, "Must be run under root (or with enough capabilities).\n");
		exit(1);
	}
	if ((err = netns_switch_root())) {
		fprintf(stderr, "Cannot change to the root name space: %s\n", strerror(err));
		exit(1);
	}

	register_handlers();

	if ((err = netns_list(&root))) {
		fprintf(stderr, "ERROR: %s\n", strerror(err));
		exit(1);
	}
	dot_output(root);
	global_handler_cleanup(root);
	netns_list_free(root);

	return 0;
}
