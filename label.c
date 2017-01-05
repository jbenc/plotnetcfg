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

#include "label.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"

int label_add(struct list *labels, char *fmt, ...)
{
	va_list ap;
	struct label *new;
	int err = ENOMEM;

	va_start(ap, fmt);
	new = calloc(sizeof(*new), 1);
	if (!new)
		goto out;
	if (vasprintf(&new->text, fmt, ap) < 0) {
		free(new);
		goto out;
	}

	err = 0;
	list_append(labels, node(new));
out:
	va_end(ap);
	return err;
}

static void label_destruct(struct label *item)
{
	free(item->text);
}

void label_free(struct list *labels)
{
	list_free(labels, (destruct_f)label_destruct);
}

int label_add_property(struct list *properties, int type,
		       const char *key, const char *fmt, ...)
{
	va_list ap;
	struct label_property *new;
	int err = ENOMEM;

	va_start(ap, fmt);
	new = calloc(1, sizeof(*new));
	if (!new)
		goto out;

	new->key = strdup(key);
	if (!new->key)
		goto out_new;

	if (vasprintf(&new->value, fmt, ap) < 0) {
		goto out_key;
	}

	new->type = type;
	list_append(properties, node(new));
	return 0;

out_key:
	free(new->key);
out_new:
	free(new);
out:
	va_end(ap);
	return err;
}

static void label_property_destruct(struct label_property *prop)
{
	free(prop->key);
	free(prop->value);
}

void label_free_property(struct list *properties)
{
	list_free(properties, (destruct_f)label_property_destruct);
}
