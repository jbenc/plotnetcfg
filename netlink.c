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

#include "netlink.h"
#include <assert.h>
#include <errno.h>
#include <linux/genetlink.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "list.h"
#include "utils.h"

#define NLMSG_BASIC_SIZE	16384

#define NL_TIMEOUT_MS		500
#define NL_RETRY_COUNT		16

int nl_open(struct nl_handle *hnd, int family)
{
	int bufsize;
	int err;
	struct sockaddr_nl sa;
	socklen_t sa_len;

	hnd->fd = socket(AF_NETLINK, SOCK_RAW, family);
	if (hnd->fd < 0)
		return -errno;
	hnd->seq = 0;
	bufsize = 32768;
	if (setsockopt(hnd->fd, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize)) < 0)
		goto err_out;
	bufsize = 1048576;
	if (setsockopt(hnd->fd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize)) < 0)
		goto err_out;

	memset(&sa, 0, sizeof(sa));
	sa.nl_family = AF_NETLINK;
	if (bind(hnd->fd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
		goto err_out;
	sa_len = sizeof(sa);
	if (getsockname(hnd->fd, (struct sockaddr *)&sa, &sa_len) < 0)
		goto err_out;
	hnd->pid = sa.nl_pid;
	return 0;

err_out:
	err = -errno;
	close(hnd->fd);
	return err;
}

void nl_close(struct nl_handle *hnd)
{
	close(hnd->fd);
}

static struct nlmsg *nlmsg_alloc(unsigned int size)
{
	struct nlmsg *msg;

	msg = calloc(1, sizeof(*msg));
	if (!msg)
		return NULL;
	msg->buf = malloc(size);
	if (!msg->buf)
		goto err_out;
	msg->allocated = size;
	return msg;

err_out:
	free(msg);
	return NULL;
}

static int nlmsg_realloc(struct nlmsg *msg, unsigned int min_len)
{
	unsigned int new_alloc = msg->allocated;
	void *new_buf;

	if (new_alloc >= min_len)
		return 0;
	while (new_alloc < min_len)
		new_alloc *= 2;
	new_buf = realloc(msg->buf, new_alloc);
	if (!new_buf)
		return ENOMEM;
	msg->buf = new_buf;
	msg->allocated = new_alloc;
	return 0;
}

void nlmsg_free(struct nlmsg *msg)
{
	struct nlmsg *next;

	while (msg) {
		next = msg->next;
		free(msg->buf);
		free(msg);
		msg = next;
	}
}

static int nlmsg_put_raw(struct nlmsg *msg, const void *data, int len, int padding)
{
	unsigned int new_len;
	int err;

	new_len = NLMSG_ALIGN(msg->len) + len + padding;
	err = nlmsg_realloc(msg, new_len);
	if (err)
		return err;
	/* zero out the alignment padding */
	memset(msg->buf + msg->len, 0, NLMSG_ALIGN(msg->len) - msg->len);
	/* put the new value */
	memcpy(msg->buf + NLMSG_ALIGN(msg->len), data, len);
	if (padding)
		memset(msg->buf + NLMSG_ALIGN(msg->len) + len, 0, padding);
	msg->len = new_len;
	return 0;
}

int nlmsg_put(struct nlmsg *msg, const void *data, int len)
{
	int err;

	err = nlmsg_put_raw(msg, data, len, 0);
	if (err)
		return err;
	nlmsg_get_hdr(msg)->nlmsg_len = msg->len;
	return 0;
}

static int nlmsg_put_padded(struct nlmsg *msg, const void *data, int len, int size)
{
	int err;

	err = nlmsg_put_raw(msg, data, len, size - len);
	if (err)
		return err;
	nlmsg_get_hdr(msg)->nlmsg_len = msg->len;
	return 0;
}

void *nlmsg_get(struct nlmsg *msg, int len)
{
	void *res;

	if (msg->len - msg->start < len)
		return NULL;
	res = msg->buf + msg->start;
	msg->start += NLMSG_ALIGN(len);
	return res;
}

void nlmsg_unget(struct nlmsg *msg, int len)
{
	len = NLMSG_ALIGN(len);
	assert(msg->start >= len);
	msg->start -= len;
}

struct nlmsg *nlmsg_new(int type, int flags)
{
	struct nlmsghdr hdr = { .nlmsg_type = type, .nlmsg_flags = flags };
	struct nlmsg *msg;

	hdr.nlmsg_flags |= NLM_F_REQUEST;
	msg = nlmsg_alloc(NLMSG_BASIC_SIZE);
	if (!msg)
		return NULL;
	if (nlmsg_put(msg, &hdr, sizeof(hdr))) {
		nlmsg_free(msg);
		return NULL;
	}
	return msg;
}

static void nlmsg_reset_start(struct nlmsg *msg)
{
	msg->start = NLMSG_ALIGN(sizeof(struct nlmsghdr));
}

struct nlattr **nlmsg_attrs(struct nlmsg *msg, int max)
{
	struct nlattr **tb;

	tb = calloc(max + 1, sizeof(struct nlattr *));
	if (!tb)
		return NULL;
	for_each_nla(a, msg) {
		if (a->nla_type <= max)
			tb[a->nla_type] = a;
	}
	return tb;
}

struct nlattr **nla_nested_attrs(struct nlattr *nla, int max)
{
	struct nlattr **tb;

	tb = calloc(max + 1, sizeof(struct nlattr *));
	if (!tb)
		return NULL;
	for_each_nla_nested(a, nla) {
		if (a->nla_type <= max)
			tb[a->nla_type] = a;
	}
	return tb;
}

int nla_put(struct nlmsg *msg, int type, const void *data, int len)
{
	struct nlattr a = { .nla_len = len + sizeof(struct nlattr),
			    .nla_type = type };

	if (nlmsg_put(msg, &a, sizeof(a)) ||
	    nlmsg_put(msg, data, len))
		return ENOMEM;
	return 0;
}

static int nl_send(struct nl_handle *hnd, struct iovec *iov, int iovlen)
{
	struct sockaddr_nl sa = {
		.nl_family = AF_NETLINK,
	};
	struct msghdr msg = {
		.msg_name = &sa,
		.msg_namelen = sizeof(sa),
		.msg_iov = iov,
		.msg_iovlen = iovlen,
	};
	struct nlmsghdr *src = iov->iov_base;

	src->nlmsg_seq = ++hnd->seq;
	if (sendmsg(hnd->fd, &msg, 0) < 0)
		return errno;
	return 0;
}

static int nl_recv(struct nl_handle *hnd, struct nlmsg **dest, int is_dump)
{
	struct sockaddr_nl sa = {
		.nl_family = AF_NETLINK,
	};
	struct iovec iov;
	struct msghdr msg = {
		.msg_name = &sa,
		.msg_namelen = sizeof(sa),
		.msg_iov = &iov,
		.msg_iovlen = 1,
	};
	char buf[16384];
	int len, err;
	struct nlmsghdr *n;
	struct nlmsg *ptr = NULL; /* GCC false positive */
	struct nlmsg *entry;
	struct pollfd pfd;

	*dest = NULL;
	pfd.fd = hnd->fd;
	pfd.events = POLLIN;
	while (1) {
		iov.iov_base = buf;
		iov.iov_len = sizeof(buf);
		err = poll(&pfd, 1, NL_TIMEOUT_MS);
		if (err < 0) {
			err = errno;
			goto err_out;
		}
		if (err == 0 || !(pfd.revents & POLLIN)) {
			err = ETIME;
			goto err_out;
		}
		len = recvmsg(hnd->fd, &msg, 0);
		if (len < 0) {
			err = errno;
			goto err_out;
		}
		if (!len) {
			err = EPIPE;
			goto err_out;
		}
		if (sa.nl_pid) {
			/* not from the kernel */
			continue;
		}
		for (n = (struct nlmsghdr *)buf; NLMSG_OK(n, len); n = NLMSG_NEXT(n, len)) {
			if (n->nlmsg_pid != hnd->pid || n->nlmsg_seq != hnd->seq)
				continue;
			if (is_dump && n->nlmsg_type == NLMSG_DONE)
				return 0;
			if (n->nlmsg_type == NLMSG_ERROR) {
				struct nlmsgerr *nlerr = (struct nlmsgerr *)NLMSG_DATA(n);

				err = -nlerr->error;
				goto err_out;
			}
			entry = nlmsg_alloc(n->nlmsg_len);
			if (!entry) {
				err = ENOMEM;
				goto err_out;
			}
			if (!*dest)
				*dest = entry;
			else
				ptr->next = entry;
			ptr = entry;

			err = nlmsg_put_raw(entry, n, n->nlmsg_len, 0);
			if (err)
				goto err_out;
			nlmsg_reset_start(entry);

			if (!is_dump)
				return 0;
		}
	}
err_out:
	nlmsg_free(*dest);
	*dest = NULL;
	return err;
}

static int nl_check_interrupted_dump(struct nlmsg *entry)
{
	for (; entry; entry = entry->next)
		if (nlmsg_get_hdr(entry)->nlmsg_flags & NLM_F_DUMP_INTR)
			return 1;
	return 0;
}

int nl_exchange(struct nl_handle *hnd, struct nlmsg *src, struct nlmsg **dest)
{
	struct iovec iov = {
		.iov_base = src->buf,
		.iov_len = src->len,
	};
	int is_dump;
	int err;
	int retry = NL_RETRY_COUNT;

	is_dump = !!(nlmsg_get_hdr(src)->nlmsg_flags & NLM_F_DUMP);
	while (1) {
		if (!retry--)
			return EINTR;

		err = nl_send(hnd, &iov, 1);
		if (err)
			return err;
		err = nl_recv(hnd, dest, is_dump);
		if (err == ETIME || err == EAGAIN || err == EINTR)
			continue;
		if (err)
			return err;

		if (is_dump && nl_check_interrupted_dump(*dest)) {
			nlmsg_free(*dest);
			*dest = NULL;
			continue;
		}

		return 0;
	}
}

int rtnl_open(struct nl_handle *hnd)
{
	return nl_open(hnd, NETLINK_ROUTE);
}

struct nlmsg *rtnlmsg_new(int type, int family, int flags, int size)
{
	struct rtgenmsg g = { .rtgen_family = family };
	struct nlmsg *res;

	res = nlmsg_new(type, flags);
	if (!res)
		return NULL;
	if (nlmsg_put_padded(res, &g, sizeof(g), size)) {
		nlmsg_free(res);
		return NULL;
	}
	return res;
}

int rtnl_ifi_dump(struct nl_handle *hnd, int type, int family, struct nlmsg **dest)
{
	struct nlmsg *req;
	int err;

	req = rtnlmsg_new(type, family, NLM_F_DUMP, sizeof(struct ifinfomsg));
	if (!req)
		return ENOMEM;
	err = nl_exchange(hnd, req, dest);
	nlmsg_free(req);
	return err;
}

int genl_open(struct nl_handle *hnd)
{
	return nl_open(hnd, NETLINK_GENERIC);
}

struct nlmsg *genlmsg_new(int type, int cmd, int flags)
{
	struct genlmsghdr g = { .cmd = cmd, .version = 1 };
	struct nlmsg *res;

	res = nlmsg_new(type, flags);
	if (!res)
		return NULL;
	if (nlmsg_put(res, &g, sizeof(g))) {
		nlmsg_free(res);
		return NULL;
	}
	return res;
}

unsigned int genl_family_id(struct nl_handle *hnd, const char *name)
{
	struct nlmsg *req, *resp;
	int res = 0;

	req = genlmsg_new(GENL_ID_CTRL, CTRL_CMD_GETFAMILY, 0);
	if (!req)
		return 0;
	if (nla_put_str(req, CTRL_ATTR_FAMILY_NAME, name))
		goto out_req;
	if (nl_exchange(hnd, req, &resp))
		goto out_req;
	if (!nlmsg_get(resp, sizeof(struct genlmsghdr)))
		goto out_resp;
	for_each_nla(a, resp) {
		if (a->nla_type == CTRL_ATTR_FAMILY_ID) {
			res = nla_read_u16(a);
			break;
		}
	}

out_resp:
	nlmsg_free(resp);
out_req:
	nlmsg_free(req);
	return res;
}
