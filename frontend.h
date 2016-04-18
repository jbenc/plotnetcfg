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

#ifndef _FRONTEND_H
#define _FRONTEND_H
#include <stdbool.h>

#include "netns.h"

struct output_entry {
	struct output_entry *next;
	char *format, *file;
	struct frontend *frontend;
	unsigned int print_mask;
	bool filter_loopback;
	bool filter_ipv6_linklocal;
};

struct frontend {
	struct frontend *next;
	const char *format;
	const char *desc;
	void (*output)(FILE *file, struct netns_entry *root, struct output_entry *output_entry);
};

void frontend_init(void);
void frontend_register(struct frontend *f);
int frontend_output(struct netns_entry *root);
void frontend_cleanup(void);

#endif
