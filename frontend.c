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
#include "if.h"
#include "utils.h"
#include "frontend.h"

static struct frontend *frontends = NULL;
static struct frontend *frontends_tail = NULL;
static int used_default_format = 0;

static struct output_entry default_output = {
	.next = NULL,
	.format = "dot",
	.file = NULL,
	.print_mask = -1U,
	.filter_loopback = false,
	.filter_ipv6_linklocal = false,
};

static struct output_entry *output = NULL;

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

	out->next = output;
	output = out;

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

	if (!output) {
		if ((err = add_output(default_output.format)))
			return err;
		used_default_format = 1;
	}

	return 0;
}

static int set_output(char *arg)
{
	int err;

	if ((err = require_format()))
		return err;

	/* Multiple --outputs for one --format: clone last output */
	if (output->file && (err = add_output(output->format)))
		return err;

	output->file = strdup(arg);
	if (!output->file)
		return ENOMEM;

	return 0;
}

static int set_nostate()
{
	int err;

	if ((err = require_format()))
		return err;

	output->print_mask &= ~IF_PROP_STATE;
	return 0;
}

static int set_loopback()
{
	int err;

	if ((err = require_format()))
		return err;

	output->filter_loopback = true;
	return 0;
}

static int set_v6_linklocal()
{
	int err;

	if ((err = require_format()))
		return err;

	output->filter_ipv6_linklocal = true;
	return 0;
}

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
	{ .long_name = "filter-loopback", .short_name = 'l', .has_arg = 0,
	  .type = ARG_CALLBACK, .action.callback = set_loopback,
	  .help = "don't output loopback interfaces (dot only)",
	},
	{ .long_name = "filter-v6-local", .short_name = 'L', .has_arg = 0,
	  .type = ARG_CALLBACK, .action.callback = set_v6_linklocal,
	  .help = "don't output v6 link-local addresses (dot only)",
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
	struct output_entry *out, *head;
	FILE *file;

	head = output ? : &default_output;

	for (out = head; out; out = out->next) {
		for (f = frontends; f; f = f->next) {
			if (!strcmp(f->format, out->format)) {
				out->frontend = f;
				break;
			}
		}
		if (!out->frontend)
			return EINVAL;
	}

	for (out = head; out; out = out->next) {
		if (out->file && strcmp("-", out->file)) {
			file = fopen(out->file, "w");
			if (!file)
				return errno;
		} else
			file = stdout;

		out->frontend->output(file, root, out);

		if (file != stdout && fclose(file))
			return errno;
	}

	return 0;
}

void frontend_cleanup()
{
	struct output_entry *next;

	while (output) {
		next = output->next;
		free(output->format);
		free(output->file);
		free(output);
		output = next;
	}
}
