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

#include <arpa/inet.h>
#include <limits.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "if.h"
#include "netns.h"
#include "utils.h"

struct generic_list {
	struct generic_list *next;
};

void list_free(void *list, destruct_f destruct)
{
	struct generic_list *cur = list, *next;

	while (cur) {
		next = cur->next;
		if (destruct)
			destruct(cur);
		free(cur);
		cur = next;
	}
}

char *ifstr(struct if_entry *entry)
{
	static char buf[IFNAMSIZ + NAME_MAX + 2];

	if (!entry->ns->name)
		/* root ns */
		snprintf(buf, sizeof(buf), "/%s", entry->if_name);
	else
		snprintf(buf, sizeof(buf), "%s/%s", entry->ns->name, entry->if_name);
	return buf;
}

char *ifid(struct if_entry *entry)
{
	static char buf[IFNAMSIZ + 2 * NAME_MAX + 6 + 1];
	char *ns;

	ns = entry->ns->name;
	if (!ns)
		/* root ns */
		ns = "";
	if (entry->internal_ns)
		snprintf(buf, sizeof(buf), "//%s/%s/%s",
			 entry->internal_ns, ns, entry->if_name);
	else
		snprintf(buf, sizeof(buf), "%s/%s",
			 ns, entry->if_name);
	return buf;
}

int raw_addr(void *dst, const char *src)
{
	int af;

	if (strchr(src, ':'))
		af = AF_INET6;
	else
		af = AF_INET;
	if (inet_pton(af, src, dst) <= 0)
		return -1;
	return af;
}
