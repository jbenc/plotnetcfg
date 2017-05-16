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

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX	108
#endif

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

#define OVS_VPORT_FAMILY	"ovs_vport"
#define OVS_VPORT_CMD_GET	3
#define OVS_VPORT_ATTR_NAME	3

struct ovs_header {
	int dp_ifindex;
};

#define IFLA_VXLAN_GROUP6		16
#define IFLA_VXLAN_LOCAL6		17
#define IFLA_VXLAN_COLLECT_METADATA	25

#if IFLA_VXLAN_MAX < IFLA_VXLAN_COLLECT_METADATA
#undef IFLA_VXLAN_MAX
#define IFLA_VXLAN_MAX IFLA_VXLAN_COLLECT_METADATA
#endif

#ifndef IFLA_BOND_MAX
enum {
	IFLA_BOND_UNSPEC,
	IFLA_BOND_MODE,
	IFLA_BOND_ACTIVE_SLAVE,
	IFLA_BOND_MIIMON,
	IFLA_BOND_UPDELAY,
	IFLA_BOND_DOWNDELAY,
	IFLA_BOND_USE_CARRIER,
	IFLA_BOND_ARP_INTERVAL,
	IFLA_BOND_ARP_IP_TARGET,
	IFLA_BOND_ARP_VALIDATE,
	IFLA_BOND_ARP_ALL_TARGETS,
	IFLA_BOND_PRIMARY,
	IFLA_BOND_PRIMARY_RESELECT,
	IFLA_BOND_FAIL_OVER_MAC,
	IFLA_BOND_XMIT_HASH_POLICY,
	IFLA_BOND_RESEND_IGMP,
	IFLA_BOND_NUM_PEER_NOTIF,
	IFLA_BOND_ALL_SLAVES_ACTIVE,
	IFLA_BOND_MIN_LINKS,
	IFLA_BOND_LP_INTERVAL,
	IFLA_BOND_PACKETS_PER_SLAVE,
	IFLA_BOND_AD_LACP_RATE,
	IFLA_BOND_AD_SELECT,
	IFLA_BOND_AD_INFO,
	__IFLA_BOND_MAX,
};
#define IFLA_BOND_MAX	(__IFLA_BOND_MAX - 1)
#endif

#ifndef RTAX_QUICKACK
#define RTAX_QUICKACK	15
#endif

#ifndef RTAX_CC_ALGO
#define RTAX_CC_ALGO	16
#endif

#if RTAX_MAX < RTAX_CC_ALGO
#undef RTAX_MAX
#define RTAX_MAX RTAX_CC_ALGO
#endif

#ifndef RTPROT_MROUTED
#define RTPROT_MROUTED	17
#endif

#ifndef RTPROT_BABEL
#define RTPROT_BABEL	42
#endif


#endif
