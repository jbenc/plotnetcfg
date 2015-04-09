/*
 * This file is a part of plotnetcfg, a tool to visualize network config.
 * Copyright (C) 2015 Red Hat, Inc. -- Jiri Benc <jbenc@redhat.com>
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

#ifndef _NETLINK_H
#define _NETLINK_H

#include <linux/netlink.h>
#include <linux/rtnetlink.h>

struct rtnl_handle {
	int fd;
	unsigned int pid;
	unsigned int seq;
};

struct nlmsg_entry {
	struct nlmsg_entry *next;
	struct nlmsghdr h;
};

int rtnl_open(struct rtnl_handle *hnd);
void rtnl_close(struct rtnl_handle *hnd);
int rtnl_exchange(struct rtnl_handle *hnd,
		  struct nlmsghdr *src, struct nlmsg_entry **dest);
int rtnl_dump(struct rtnl_handle *hnd, int family, int type, struct nlmsg_entry **dest);
void rtnl_parse(struct rtattr *tb[], int max, struct rtattr *rta, int len);
void rtnl_parse_nested(struct rtattr *tb[], int max, struct rtattr *rta);

void nlmsg_free(struct nlmsg_entry *entry);

#endif
