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

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include "utils.h"
#include "netlink.h"

int rtnl_open(struct rtnl_handle *hnd)
{
	int bufsize;
	int err;
	struct sockaddr_nl sa;
	socklen_t sa_len;

	hnd->fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
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

void rtnl_close(struct rtnl_handle *hnd)
{
	close(hnd->fd);
}

void nlmsg_free(struct nlmsg_entry *entry)
{
	list_free(entry, NULL);
}

int rtnl_exchange(struct rtnl_handle *hnd,
		  struct nlmsghdr *src, struct nlmsg_entry **dest)
{
	struct sockaddr_nl sa = {
		.nl_family = AF_NETLINK,
	};
	struct iovec iov = {
		.iov_base = src,
		.iov_len = src->nlmsg_len,
	};
	struct msghdr msg = {
		.msg_name = &sa,
		.msg_namelen = sizeof(sa),
		.msg_iov = &iov,
		.msg_iovlen = 1,
	};
	char buf[16384];
	int is_dump;
	int len, err;
	struct nlmsghdr *n;
	struct nlmsg_entry *ptr, *entry;

	*dest = NULL;

	src->nlmsg_seq = ++hnd->seq;
	is_dump = !!(src->nlmsg_flags & NLM_F_DUMP);
	if (sendmsg(hnd->fd, &msg, 0) < 0)
		return errno;

	while (1) {
		iov.iov_base = buf;
		iov.iov_len = sizeof(buf);
		len = recvmsg(hnd->fd, &msg, 0);
		if (len < 0)
			return errno;
		if (!len)
			return EPIPE;
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

int rtnl_dump(struct rtnl_handle *hnd, int family, int type, struct nlmsg_entry **dest)
{
	struct {
		struct nlmsghdr n;
		struct ifinfomsg i;
	} req;

	memset(&req, 0, sizeof(req));
	req.n.nlmsg_len = sizeof(req);
	req.n.nlmsg_type = type;
	req.n.nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST;
	req.n.nlmsg_seq = ++hnd->seq;
	req.i.ifi_family = family;
	return rtnl_exchange(hnd, &req.n, dest);
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
