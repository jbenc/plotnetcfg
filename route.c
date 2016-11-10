/*
 * This file is a part of plotnetcfg; a tool to visualize network config.
 * Copyright (C) 2016 Red Hat; Inc. -- Ondrej Hlavaty <ohlavaty@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License; or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful;
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "route.h"
#include <stdio.h>
#include "compat.h"

static const char *route_unknown(int num)
{
	static char buf [64];
	snprintf(buf, sizeof(buf), "unknown (%u)", num);
	return buf;
}

const char *route_metric(int metric)
{
	switch (metric) {
	case RTAX_MTU:		return "mtu";
	case RTAX_WINDOW:	return "window";
	case RTAX_RTT:		return "rtt";
	case RTAX_RTTVAR:	return "rttvar";
	case RTAX_SSTHRESH:	return "ssthresh";
	case RTAX_CWND:		return "cwnd";
	case RTAX_ADVMSS:	return "advmss";
	case RTAX_REORDERING:	return "reordering";
	case RTAX_HOPLIMIT:	return "hoplimit";
	case RTAX_INITCWND:	return "initcwnd";
	case RTAX_FEATURES:	return "features";
	case RTAX_RTO_MIN:	return "rto_min";
	case RTAX_INITRWND:	return "initrwnd";
	case RTAX_QUICKACK:	return "quickack";
	case RTAX_CC_ALGO:	return "congctl";
	}
	return route_unknown(metric);
}

const char *route_protocol(int protocol)
{
	switch (protocol) {
	case RTPROT_UNSPEC:	return "unspec";
	case RTPROT_REDIRECT:	return "redirect";
	case RTPROT_KERNEL:	return "kernel";
	case RTPROT_BOOT:	return "boot";
	case RTPROT_STATIC:	return "static";
	case RTPROT_GATED:	return "gated";
	case RTPROT_RA:		return "ra";
	case RTPROT_MRT:	return "mrt";
	case RTPROT_ZEBRA:	return "zebra";
	case RTPROT_BIRD:	return "bird";
	case RTPROT_DNROUTED:	return "dnrouted";
	case RTPROT_XORP:	return "xorp";
	case RTPROT_NTK:	return "ntk";
	case RTPROT_DHCP:	return "dhcp";
	case RTPROT_MROUTED:	return "mrouted";
	case RTPROT_BABEL:	return "babel";
	}
	return route_unknown(protocol);
}

const char *route_scope(int scope)
{
	switch (scope) {
	case RT_SCOPE_UNIVERSE: return "universe";
	case RT_SCOPE_SITE: return "site";
	case RT_SCOPE_LINK: return "link";
	case RT_SCOPE_HOST: return "host";
	}
	return route_unknown(scope);
}

const char *route_table(int table)
{
	switch (table) {
	case RT_TABLE_UNSPEC:	return "unspec";
	case RT_TABLE_COMPAT:	return "compat";
	case RT_TABLE_DEFAULT:	return "default";
	case RT_TABLE_MAIN:	return "main";
	case RT_TABLE_LOCAL:	return "local";
	}
	return route_unknown(table);
}

const char *route_type(int type)
{
	switch (type) {
	case RTN_UNSPEC:	return "unspec";
	case RTN_UNICAST:	return "unicast";
	case RTN_LOCAL:		return "local";
	case RTN_BROADCAST:	return "broadcast";
	case RTN_ANYCAST:	return "anycast";
	case RTN_MULTICAST:	return "multicast";
	case RTN_BLACKHOLE:	return "blackhole";
	case RTN_UNREACHABLE:	return "unreachable";
	case RTN_PROHIBIT:	return "prohibit";
	case RTN_THROW:		return "throw";
	case RTN_NAT:		return "nat";
	}
	return route_unknown(type);
}
