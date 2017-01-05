/*
 * This file is a part of plotnetcfg, a tool to visualize network config.
 * Copyright (C) 2016 Red Hat, Inc. -- Ondrej Hlavaty <ohlavaty@redhat.com>
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

#ifndef _ROUTE_H
#define _ROUTE_H

#include "addr.h"
#include "list.h"
#include <sys/socket.h>
#include <linux/rtnetlink.h>

struct if_entry;

struct rtmetric {
	struct node n;
	int type, value;
};

struct route {
	struct node n;
	struct if_entry *iif, *oif;
	struct list metrics;
	struct addr dst, gw, prefsrc, src;
	unsigned int iifindex, oifindex, priority;
	unsigned char table_id, tos, protocol, type, family, scope;
};

struct rtable {
	struct node n;
	int id;
	struct list routes;
};

const char *route_metric(int type);
const char *route_protocol(int protocol);
const char *route_scope(int scope);
const char *route_table(int table);
const char *route_type(int type);

#endif
