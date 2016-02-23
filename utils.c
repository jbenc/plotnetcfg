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

#define INTERNAL_NS_MAX (NAME_MAX)
#define NETNS_MAX (NAME_MAX + 1)
#define IFID_MAX (INTERNAL_NS_MAX + NETNS_MAX + IFNAMSIZ + 1)
char *ifid(struct if_entry *entry)
{
	static char buf[IFID_MAX + 1];
	char *ins, *ns;

	ins = entry->internal_ns ? : "";
	ns = nsid(entry->ns);

	snprintf(buf, sizeof(buf), "%s%s/%s", ns, ins, entry->if_name);
	return buf;
}

char *nsid(struct netns_entry *entry)
{
	static char buf[NETNS_MAX + 1];

	if (!entry->name)
		return "/";

	snprintf(buf, sizeof(buf), "%s/", entry->name);
	return buf;
}
