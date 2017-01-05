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

#include "frontend.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "args.h"
#include "if.h"
#include "utils.h"

static struct output_entry default_output = {
	.format = "dot",
	.file = NULL,
	.print_mask = -1U,
};

static int used_default_format = 0;

static DECLARE_LIST(outputs);
static DECLARE_LIST(frontends);

static int add_output(char *format)
{
	struct output_entry *out;

	out = malloc(sizeof(struct output_entry));
	if (!out)
		return ENOMEM;

	*out = default_output;

	out->format = strdup(format);
	if (!out->format) {
		free(out);
		return ENOMEM;
	}

	list_prepend(&outputs, node(out));

	return 0;
}

static int add_format(char *arg)
{
	if (used_default_format) {
		fprintf(stderr, "Failed to parse arguments: output options must come after --format.\n");
		return EINVAL;
	}

	return add_output(arg);
}

static int require_format()
{
	int err;

	if (list_empty(outputs)) {
		if ((err = add_output(default_output.format)))
			return err;
		used_default_format = 1;
	}

	return 0;
}

static int set_output(char *arg)
{
	int err;
	struct output_entry *head;

	if ((err = require_format()))
		return err;

	head = list_head(outputs);
	/* Multiple --outputs for one --format: clone last output */
	if (head->file && (err = add_output(head->format)))
		return err;

	head = list_head(outputs);
	head->file = strdup(arg);
	if (!head->file)
		return ENOMEM;

	return 0;
}

static int set_nostate()
{
	int err;
	struct output_entry *head;

	if ((err = require_format()))
		return err;

	head = list_head(outputs);
	head->print_mask &= ~IF_PROP_STATE;
	return 0;
}

static int print_formats(_unused char *arg)
{
	struct frontend *f;

	printf("Available output formats:\n");
	list_for_each(f, frontends)
		printf("\t%s - %s\n", f->format, f->desc);
	return 1;
}

static struct arg_option options[] = {
	{ .long_name = "format", .short_name = 'f', .has_arg = 1,
	  .type = ARG_CALLBACK, .action.callback = add_format,
	  .help = "output format (default: dot)",
	},
	{ .long_name = "output", .short_name = 'o', .has_arg = 1,
	  .type = ARG_CALLBACK, .action.callback = set_output,
	  .help = "output file (default: standart output)",
	},
	{ .long_name = "config-only", .short_name = 'C', .has_arg = 0,
	  .type = ARG_CALLBACK, .action.callback = set_nostate,
	  .help = "skip state in output, print only configuration",
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
	list_append(&frontends, node(f));
}

int frontend_output(struct list *netns_list)
{
	struct frontend *f;
	struct output_entry *out;
	FILE *file;

	require_format();

	list_for_each(out, outputs) {
		list_for_each(f, frontends) {
			if (!strcmp(f->format, out->format)) {
				out->frontend = f;
				break;
			}
		}
		if (!out->frontend)
			return EINVAL;
	}

	list_for_each(out, outputs) {
		if (out->file && strcmp("-", out->file)) {
			file = fopen(out->file, "w");
			if (!file)
				return errno;
		} else
			file = stdout;

		out->frontend->output(file, netns_list, out);

		if (file != stdout && fclose(file))
			return errno;
	}

	return 0;
}

void frontend_cleanup()
{
	struct output_entry *out;

	while ((out = list_pop(&outputs))) {
		free(out->format);
		free(out->file);
		free(out);
	}
}
