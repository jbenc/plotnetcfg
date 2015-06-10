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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "args.h"
#include "utils.h"
#include "frontend.h"

static struct frontend *frontends = NULL;
static struct frontend *frontends_tail = NULL;
static char *format = "dot";

static int print_formats(_unused char *arg)
{
	struct frontend *f;

	printf("Available output formats:\n");
	for (f = frontends; f; f = f->next)
		printf("\t%s - %s\n", f->format, f->desc);
	return 1;
}

static struct arg_option options[] = {
	{ .long_name = "format", .short_name = 'f', .has_arg = 1,
	  .type = ARG_CHAR, .action.char_var = &format,
	  .help = "output format (dot by default)",
	},
	{ .long_name = "list-formats", .short_name = 'F',
	  .type = ARG_CALLBACK, .action.callback = print_formats,
	  .help = "list available output formats",
	},
};

void frontend_init(void)
{
	arg_register_batch(options, ARRAY_SIZE(options));
}

void frontend_register(struct frontend *f)
{
	f->next = NULL;
	if (!frontends) {
		frontends = frontends_tail = f;
		return;
	}
	frontends_tail->next = f;
	frontends_tail = f;
}

int frontend_output(struct netns_entry *root)
{
	struct frontend *f;

	for (f = frontends; f; f = f->next) {
		if (!strcmp(f->format, format)) {
			f->output(root);
			return 0;
		}
	}
	return EINVAL;
}
