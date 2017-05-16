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

#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
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
struct nlattr **nla_attrs(struct nlattr *nla, int len, int max);
struct nlattr **nla_nested_attrs(struct nlattr *nla, int max);

int nla_add_str(void *orig, int orig_len, int nla_type, const char *str,
		void **dest);

static inline unsigned int nla_len(const struct nlattr *nla)
{
	return nla->nla_len - sizeof(struct nlattr);
}

static inline const void *nla_read(const struct nlattr *nla)
{
	return nla + 1;
}

static inline uint8_t nla_read_u8(const struct nlattr *nla)
{
	return *(uint8_t *)(nla + 1);
}

static inline uint16_t nla_read_u16(const struct nlattr *nla)
{
	return *(uint16_t *)(nla + 1);
}

static inline uint32_t nla_read_u32(const struct nlattr *nla)
{
	return *(uint32_t *)(nla + 1);
}

static inline int32_t nla_read_s32(const struct nlattr *nla)
{
	return *(int32_t *)(nla + 1);
}

static inline const char *nla_read_str(const struct nlattr *nla)
{
	return (const char *)(nla + 1);
}

/* rtnetlink */

int rtnl_open(struct nl_handle *hnd);
int rtnl_dump(struct nl_handle *hnd, int family, int type, struct nlmsg_entry **dest);

/* genetlink */

int genl_open(struct nl_handle *hnd);
int genl_request(struct nl_handle *hnd,
		 int type, int cmd, void *payload, int payload_len,
		 struct nlmsg_entry **dest);
unsigned int genl_family_id(struct nl_handle *hnd, const char *name);

#endif
