/*
 * This file is a part of plotnetcfg, a tool to visualize network config.
 * Copyright (C) 2014 Red Hat, Inc. -- Jiri Benc <jbenc@redhat.com>
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

#ifndef _TUNNEL_H
#define _TUNNEL_H

struct addr;
struct if_entry;
struct netns_entry;

struct if_entry *tunnel_find_str(struct netns_entry *ns, const char *addr);
struct if_entry *tunnel_find_addr(struct netns_entry *ns, struct addr *addr);

#endif
