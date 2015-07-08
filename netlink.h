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

struct nl_handle {
	int fd;
	unsigned int pid;
	unsigned int seq;
};

struct nlmsg_entry {
	struct nlmsg_entry *next;
	struct nlmsghdr h;
};

/* all netlink families */

void nl_close(struct nl_handle *hnd);
int nl_exchange(struct nl_handle *hnd,
		struct nlmsghdr *src, struct nlmsg_entry **dest);
void nlmsg_free(struct nlmsg_entry *entry);

int nla_add_str(void *orig, int orig_len, int nla_type, const char *str,
		void **dest);

/* rtnetlink */

int rtnl_open(struct nl_handle *hnd);
int rtnl_dump(struct nl_handle *hnd, int family, int type, struct nlmsg_entry **dest);
void rtnl_parse(struct rtattr *tb[], int max, struct rtattr *rta, int len);
void rtnl_parse_nested(struct rtattr *tb[], int max, struct rtattr *rta);

/* genetlink */

int genl_open(struct nl_handle *hnd);
int genl_request(struct nl_handle *hnd,
		 int type, int cmd, void *payload, int payload_len,
		 struct nlmsg_entry **dest);
unsigned int genl_family_id(struct nl_handle *hnd, const char *name);

#endif
