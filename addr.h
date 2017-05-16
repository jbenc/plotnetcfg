/*
 * This file is a part of plotnetcfg, a tool to visualize network config.
 * Copyright (C) 2016 Red Hat, Inc. -- Jiri Benc <jbenc@redhat.com>
 *                                     Ondrej Hlavaty <ohlavaty@redhat.com>
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

#ifndef _ADDR_H
#define _ADDR_H

#include <arpa/inet.h>

struct ifaddrmsg;
struct nlattr;

struct addr {
	int family;
	int prefixlen;
	char *formatted;
	void *raw;
};

struct mac_addr {
	int len;
	unsigned char *raw;
	char *formatted;
};

int addr_init(struct addr *addr, int ai_family, int prefixlen, const void *raw);
int addr_init_netlink(struct addr *dest, const struct ifaddrmsg *ifa,
		      const struct nlattr *nla);

/* dest must point to at least 16 bytes long buffer */
int addr_parse_raw(void *dest, const char *str);

static inline int addr_max_prefix_len(int ai_family)
{
	switch (ai_family) {
		case AF_INET: return 32;
		case AF_INET6: return 128;
	}
	return 0;
}

void addr_destruct(struct addr *addr);

int mac_addr_init(struct mac_addr *addr);
int mac_addr_fill_netlink(struct mac_addr *addr, const struct nlattr *nla);

void mac_addr_destruct(struct mac_addr *addr);
#endif
