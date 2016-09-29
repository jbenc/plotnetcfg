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

#include "ethtool.h"
#include <errno.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static int ethtool_ioctl(const char *ifname, void *data)
{
	struct ifreq ifr;
	int fd, err = 0;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		return errno;
	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, ifname);
	ifr.ifr_data = data;
	if (ioctl(fd, SIOCETHTOOL, &ifr) < 0)
		err = errno;
	close(fd);
	return err;
}

char *ethtool_driver(const char *ifname)
{
	struct ethtool_drvinfo info;

	memset(&info, 0, sizeof(info));
	info.cmd = ETHTOOL_GDRVINFO;
	if (ethtool_ioctl(ifname, &info))
		return NULL;
	return strdup(info.driver);
}

unsigned int ethtool_veth_peer(const char *ifname)
{
	struct ethtool_drvinfo info;
	struct ethtool_gstrings *strs;
	struct ethtool_stats *stats;
	unsigned int i, res = 0;

	memset(&info, 0, sizeof(info));
	info.cmd = ETHTOOL_GDRVINFO;
	if (ethtool_ioctl(ifname, &info))
		return 0;
	if (!info.n_stats)
		return 0;

	strs = malloc(sizeof(struct ethtool_gstrings) + info.n_stats * ETH_GSTRING_LEN);
	if (!strs)
		return 0;
	memset(strs, 0, sizeof(struct ethtool_gstrings));
	strs->cmd = ETHTOOL_GSTRINGS;
	strs->string_set = ETH_SS_STATS;
	strs->len = info.n_stats;
	if (ethtool_ioctl(ifname, strs))
		goto fail_strs;
	if (strs->len != info.n_stats)
		goto fail_strs;

	stats = malloc(sizeof(struct ethtool_stats) + info.n_stats * sizeof(__u64));
	if (!stats)
		goto fail_strs;
	memset(stats, 0, sizeof(struct ethtool_stats));
	stats->cmd = ETHTOOL_GSTATS;
	stats->n_stats = info.n_stats;
	if (ethtool_ioctl(ifname, stats))
		goto fail_stats;
	if (stats->n_stats != info.n_stats)
		goto fail_stats;

	for (i = 0; i < info.n_stats; i++) {
		if (!strcmp((char *)strs->data + i * ETH_GSTRING_LEN, "peer_ifindex")) {
			res = stats->data[i];
			break;
		}
	}

fail_stats:
	free(stats);
fail_strs:
	free(strs);
	return res;
}
