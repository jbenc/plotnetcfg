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

#ifndef _COMPAT_H
#define _COMPAT_H

#define IFLA_LINK_NETNSID	37

#if IFLA_MAX < IFLA_LINK_NETNSID
#undef IFLA_MAX
#define IFLA_MAX IFLA_LINK_NETNSID
#endif

#define NETNSA_NSID		1
#define NETNSA_FD		3

#if NETNSA_MAX < NETNSA_FD
#undef NETNSA_MAX
#define NETNSA_MAX NETNSA_FD
#endif

#ifndef RTM_GETNSID
#define RTM_GETNSID		90
#endif

#if RTM_MAX < RTM_GETNSID
#undef RTM_MAX
#define RTM_MAX (((RTM_GETNSID + 4) & ~3) - 1)
#endif

#ifndef NETNS_RTA
#define NETNS_RTA(r) \
	((struct rtattr*)(((char*)(r)) + NLMSG_ALIGN(sizeof(struct rtgenmsg))))
#endif

#define OVS_VPORT_FAMILY	"ovs_vport"
#define OVS_VPORT_CMD_GET	3
#define OVS_VPORT_ATTR_NAME	3

struct ovs_header {
	int dp_ifindex;
};

#endif
