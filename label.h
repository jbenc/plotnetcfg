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

struct label {
	struct label *next;
	char *text;
};

struct label_property {
	struct label_property *next;
	int type;
	char *key, *value;
};

int label_add(struct label **list, char *fmt, ...);
void label_free(struct label *list);

int label_add_property(struct label_property **list, int type,
		       const char *key, const char *fmt, ...);
void label_free_property(struct label_property *list);
#define label_prop_match_mask(type, mask) (((type) & (mask)) > 0)

#endif
