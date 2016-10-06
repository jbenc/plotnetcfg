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

#include "addr.h"
#include <arpa/inet.h>
#include <errno.h>
#include <linux/rtnetlink.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

int addr_init(struct addr *dest, int family, int prefixlen, void *raw)
{
	char buf[64];
	unsigned int len = family == AF_INET ? 4 : 16;

	dest->family = family;
	dest->prefixlen = prefixlen;
	dest->raw = malloc(len);
	if (!dest->raw)
		goto err;
	memcpy(dest->raw, raw, len);

	if (!inet_ntop(family, raw, buf, sizeof(buf)))
		return errno;
	len = strlen(buf);
	snprintf(buf + len, sizeof(buf) - len, "/%d", prefixlen);
	dest->formatted = strdup(buf);
	if (!dest->formatted)
		goto err_raw;

	return 0;

err_raw:
	free(dest->raw);
err:
	return ENOMEM;
}

int addr_init_netlink(struct addr *dest, const struct ifaddrmsg *ifa,
		      const struct rtattr *rta)
{
	return addr_init(dest, ifa->ifa_family, ifa->ifa_prefixlen, RTA_DATA(rta));
}

int addr_parse_raw(void *dest, const char *src)
{
	int af;

	af = strchr(src, ':') ? AF_INET6 : AF_INET;
	if (inet_pton(af, src, dest) <= 0)
		return -1;

	return af;
}

void addr_destruct(struct addr *addr)
{
	if (addr->raw) {
		free(addr->raw);
		free(addr->formatted);
		addr->raw = addr->formatted = NULL;
	}
}

int mac_addr_init(struct mac_addr *addr)
{
	addr->len = 0;
	addr->raw = NULL;
	addr->formatted = NULL;
	return 0;
}

int mac_addr_fill_netlink(struct mac_addr *addr, const unsigned char *data, int len)
{
	int i;

	addr->len = len;

	addr->raw = malloc(len);
	if (!addr->raw)
		return ENOMEM;

	addr->formatted = malloc(len * 3);
	if (!addr->raw)
		goto err_raw;

	memcpy(addr->raw, data, len);

	for (i = 0; i < len; i++)
		if (3 != snprintf(addr->formatted + i * 3, (i+1 == len) ? 3 : 4 , "%02x:", data[i]))
			goto err_formatted;

	return 0;

err_formatted:
	free(addr->formatted);
	addr->formatted = NULL;
err_raw:
	free(addr->raw);
	addr->raw = NULL;
	addr->len = 0;

	return errno;
}

void mac_addr_destruct(struct mac_addr *addr)
{
	if (addr->raw)
		free(addr->raw);
	if (addr->formatted)
		free(addr->formatted);
}
