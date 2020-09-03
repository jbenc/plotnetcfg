/*
 * This file is a part of plotnetcfg, a tool to visualize network config.
 * Copyright (C) 2015-2017 Red Hat, Inc. -- Jiri Benc <jbenc@redhat.com>
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
#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

struct nl_handle {
	int fd;
	unsigned int pid;
	unsigned int seq;
};

struct nlmsg {
	struct nlmsg *next;
	void *buf;
	int start;
	int len;
	int allocated;
};

#define nlmsg_get_hdr(n)	((struct nlmsghdr *)(n)->buf)

/* all netlink families */

int nl_open(struct nl_handle *hnd, int family);
void nl_close(struct nl_handle *hnd);
int nl_exchange(struct nl_handle *hnd, struct nlmsg *src, struct nlmsg **dest);

struct nlmsg *nlmsg_new(int type, int flags);
void nlmsg_free(struct nlmsg *msg);
int nlmsg_put(struct nlmsg *msg, const void *data, int len);
void *nlmsg_get(struct nlmsg *msg, int len);
void nlmsg_unget(struct nlmsg *msg, int len);
struct nlattr **nlmsg_attrs(struct nlmsg *msg, int max);
struct nlattr **nla_nested_attrs(struct nlattr *nla, int max);

#define for_each_nlmsg(iter, msg)				\
	for (struct nlmsg *iter = (msg); iter; iter = iter->next)

int nla_put(struct nlmsg *msg, int type, const void *data, int len);

static inline int nla_put_u8(struct nlmsg *msg, int type, uint8_t data)
{
	return nla_put(msg, type, &data, sizeof(data));
}

static inline int nla_put_u16(struct nlmsg *msg, int type, uint16_t data)
{
	return nla_put(msg, type, &data, sizeof(data));
}

static inline int nla_put_u32(struct nlmsg *msg, int type, uint32_t data)
{
	return nla_put(msg, type, &data, sizeof(data));
}

static inline int nla_put_s32(struct nlmsg *msg, int type, int32_t data)
{
	return nla_put(msg, type, &data, sizeof(data));
}

static inline int nla_put_str(struct nlmsg *msg, int type, const char *str)
{
	return nla_put(msg, type, str, strlen(str) + 1);
}

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

static inline uint16_t nla_read_be16(const struct nlattr *nla)
{
	return ntohs(*(uint16_t *)(nla + 1));
}

static inline uint32_t nla_read_be32(const struct nlattr *nla)
{
	return ntohl(*(uint32_t *)(nla + 1));
}

#define ntohll(x) ((1==ntohl(1)) ? (x) : ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))
static inline uint64_t nla_read_be64(const struct nlattr *nla)
{
	return ntohll(*(uint64_t *)(nla + 1));
}

static inline int32_t nla_read_s32(const struct nlattr *nla)
{
	return *(int32_t *)(nla + 1);
}

static inline const char *nla_read_str(const struct nlattr *nla)
{
	return (const char *)(nla + 1);
}

#define __nla_buf_remaining(iter, buf, len)				\
	((int)(len) - ((void *)(iter) - (void *)(buf)))

#define for_each_nla_buf(iter, buf, len)				\
	for (struct nlattr *iter = (struct nlattr *)(buf);		\
	     __nla_buf_remaining(iter, buf, len) >=			\
				(int)sizeof(struct nlattr) &&		\
	     iter->nla_len >= sizeof(struct nlattr) &&			\
	     iter->nla_len <= __nla_buf_remaining(iter, buf, len);	\
	     iter = (void *)iter + NLMSG_ALIGN(iter->nla_len))

#define for_each_nla(iter, msg)	\
	for_each_nla_buf(iter, (msg)->buf + (msg)->start, (msg)->len - (msg)->start)

#define for_each_nla_nested(iter, nla)	\
	for_each_nla_buf(iter, nla_read(nla), nla_len(nla))

/* rtnetlink */

int rtnl_open(struct nl_handle *hnd);
struct nlmsg *rtnlmsg_new(int type, int family, int flags, int size);
int rtnl_ifi_dump(struct nl_handle *hnd, int type, int family, struct nlmsg **dest);

/* genetlink */

int genl_open(struct nl_handle *hnd);
struct nlmsg *genlmsg_new(int type, int cmd, int flags);
unsigned int genl_family_id(struct nl_handle *hnd, const char *name);

#endif
