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
#include <fcntl.h>
#include <net/if.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include "../handler.h"
#include "bridge.h"

static int bridge_scan(struct if_entry *entry);

static struct handler h_bridge = {
	.scan = bridge_scan,
};

void handler_bridge_register(void)
{
	handler_register(&h_bridge);
}

static int bridge_scan(struct if_entry *entry)
{
	int fd, res;
	char buf[37 + IFNAMSIZ + 1];

	if (entry->master_index)
		return 0;
	snprintf(buf, sizeof(buf), "/sys/class/net/%s/brport/bridge/ifindex", entry->if_name);
	fd = open(buf, O_RDONLY);
	if (fd < 0) {
		if (errno == ENOENT)
			return 0;
		return errno;
	}
	res = read(fd, buf, sizeof(buf));
	if (res < 0) {
		res = errno;
		goto out;
	}
	if (res == 0) {
		res = EINVAL;
		goto out;
	}
	entry->master_index = atoi(buf);
	res = 0;
out:
	close(fd);
	return res;
}
