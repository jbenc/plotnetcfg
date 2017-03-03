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

#include "netlink.h"
#include <errno.h>
#include <linux/genetlink.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "list.h"
#include "utils.h"

static int nl_open(struct nl_handle *hnd, int family)
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

void nlmsg_free(struct nlmsg_entry *entry)
{
	slist_free(entry, NULL);
}

int nl_send(struct nl_handle *hnd, struct iovec *iov, int iovlen)
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

int nl_recv(struct nl_handle *hnd, struct nlmsg_entry **dest, int is_dump)
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
	struct nlmsg_entry *ptr = NULL; /* GCC false positive */
	struct nlmsg_entry *entry;

	*dest = NULL;
	while (1) {
		iov.iov_base = buf;
		iov.iov_len = sizeof(buf);
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
			entry = malloc(n->nlmsg_len + sizeof(void *));
			if (!entry) {
				err = ENOMEM;
				goto err_out;
			}
			entry->next = NULL;
			memcpy(&entry->h, n, n->nlmsg_len);
			if (!*dest)
				*dest = entry;
			else
				ptr->next = entry;
			ptr = entry;
			if (!is_dump)
				return 0;
		}
	}
err_out:
	nlmsg_free(*dest);
	*dest = NULL;
	return err;
}

int nl_check_interrupted_dump(struct nlmsg_entry *entry)
{
	for (; entry; entry = entry->next)
		if (entry->h.nlmsg_flags & NLM_F_DUMP_INTR)
			return 1;
	return 0;
}

int nl_exchange(struct nl_handle *hnd,
		struct nlmsghdr *src, struct nlmsg_entry **dest)
{
	struct iovec iov = {
		.iov_base = src,
		.iov_len = src->nlmsg_len,
	};
	int is_dump;
	int err;
	int retry = 16;

	is_dump = !!(src->nlmsg_flags & NLM_F_DUMP);
	while (1) {
		if (!retry--)
			return EINTR;

		err = nl_send(hnd, &iov, 1);
		if (err)
			return err;
		err = nl_recv(hnd, dest, is_dump);
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

/* The original payload is not freed. Returns 0 in case of error, length
 * of *dest otherwise. *dest is newly allocated. */
int nla_add_str(void *orig, int orig_len, int nla_type, const char *str,
		void **dest)
{
	struct nlattr *nla;
	int len = strlen(str) + 1;
	int size;

	size = NLA_ALIGN(orig_len) + NLA_HDRLEN + NLA_ALIGN(len);
	*dest = calloc(1, size);
	if (!*dest)
		return 0;
	if (orig_len)
		memcpy(*dest, orig, orig_len);
	nla = *dest + NLA_ALIGN(orig_len);
	nla->nla_len = NLA_HDRLEN + len;
	nla->nla_type = nla_type;
	memcpy(nla + 1, str, len);
	return size;
}

int rtnl_open(struct nl_handle *hnd)
{
	return nl_open(hnd, NETLINK_ROUTE);
}

int rtnl_dump(struct nl_handle *hnd, int family, int type, struct nlmsg_entry **dest)
{
	struct {
		struct nlmsghdr n;
		struct ifinfomsg i;
	} req;

	memset(&req, 0, sizeof(req));
	req.n.nlmsg_len = sizeof(req);
	req.n.nlmsg_type = type;
	req.n.nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST;
	req.i.ifi_family = family;
	return nl_exchange(hnd, &req.n, dest);
}

void rtnl_parse(struct rtattr *tb[], int max, struct rtattr *rta, int len)
{
	memset(tb, 0, sizeof(struct rtattr *) * (max + 1));
	while (RTA_OK(rta, len)) {
		if (rta->rta_type <= max)
			tb[rta->rta_type] = rta;
		rta = RTA_NEXT(rta, len);
	}
}

void rtnl_parse_nested(struct rtattr *tb[], int max, struct rtattr *rta)
{
	rtnl_parse(tb, max, RTA_DATA(rta), RTA_PAYLOAD(rta));
}

int genl_open(struct nl_handle *hnd)
{
	return nl_open(hnd, NETLINK_GENERIC);
}

int genl_request(struct nl_handle *hnd,
		 int type, int cmd, void *payload, int payload_len,
		 struct nlmsg_entry **dest)
{
	struct {
		struct nlmsghdr n;
		struct genlmsghdr g;
	} req;
	struct iovec iov[2];
	int err;

	memset(&req, 0, sizeof(req));
	req.n.nlmsg_len = sizeof(req) + payload_len;
	req.n.nlmsg_type = type;
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.g.cmd = cmd;
	req.g.version = 1;

	iov[0].iov_base = &req;
	iov[0].iov_len = sizeof(req);
	iov[1].iov_base = payload;
	iov[1].iov_len = payload_len;
	err = nl_send(hnd, iov, 2);
	if (err)
		return err;
	return nl_recv(hnd, dest, 0);
}

unsigned int genl_family_id(struct nl_handle *hnd, const char *name)
{
	unsigned int res = 0;
	struct nlattr *nla;
	int len;
	struct nlmsg_entry *dest;
	void *ptr;

	len = nla_add_str(NULL, 0, CTRL_ATTR_FAMILY_NAME, name, &ptr);
	if (!len)
		return 0;
	if (genl_request(hnd, GENL_ID_CTRL, CTRL_CMD_GETFAMILY,
			 ptr, len, &dest)) {
		free(ptr);
		return 0;
	}
	free(ptr);

	len = dest->h.nlmsg_len - NLMSG_HDRLEN - GENL_HDRLEN;
	ptr = (void *)&dest->h + NLMSG_HDRLEN + GENL_HDRLEN;

	while (len > NLA_HDRLEN) {
		nla = ptr;
		if (nla->nla_type == CTRL_ATTR_FAMILY_ID &&
		    nla->nla_len >= NLA_HDRLEN + 2) {
			res = *(uint16_t *)(nla + 1);
			break;
		}

		ptr += NLMSG_ALIGN(nla->nla_len);
		len -= NLMSG_ALIGN(nla->nla_len);
	}

	nlmsg_free(dest);
	return res;

}
