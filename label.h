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

#ifndef _LABEL_H
#define _LABEL_H

#include "list.h"

struct label {
	struct node n;
	char *text;
};

struct label_property {
	struct node n;
	int type;
	char *key, *value;
};

int label_add(struct list *labels, char *fmt, ...);
void label_free(struct list *labels);

int label_add_property(struct list *properties, int type,
		       const char *key, const char *fmt, ...);
void label_free_property(struct list *properties);
#define label_prop_match_mask(type, mask) (((type) & (mask)) > 0)

#endif
