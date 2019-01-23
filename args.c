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

#include "args.h"
#include <assert.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

struct {
	struct list list;
	int count;
} options = {
	.list = LIST_INITIALIZER(options.list),
	.count = 0
};

void arg_register(struct arg_option *opt)
{
	options.count++;
	list_append(&options.list, node(opt));
}

void arg_register_batch(struct arg_option *opt, int count)
{
	int i;

	for (i = 0; i < count; i++)
		arg_register(opt + i);
}

int arg_parse(int argc, char **argv)
{
	struct option *glong;
	char *gshort;
	struct arg_option *opt;
	int glong_index, gshort_index, i;
	int res = 0;

	/* construct parameter lists for getopt */
	glong = calloc(options.count + 1, sizeof(*glong));
	gshort = calloc(options.count * 3 + 1, 1);
	if (!glong || !gshort)
		return 1;
	glong_index = gshort_index = 0;
	list_for_each(opt, options.list) {
		if (opt->long_name) {
			glong[glong_index].name = opt->long_name;
			glong[glong_index].has_arg = opt->has_arg;
			glong_index++;
		}
		if (opt->short_name) {
			gshort[gshort_index++] = opt->short_name;
			for (i = 0; i < opt->has_arg; i++)
				gshort[gshort_index++] = ':';
		}
	}

	while (1) {
		gshort_index = getopt_long(argc, argv, gshort, glong, &glong_index);
		if (gshort_index < 0)
			break;
		if (gshort_index == '?' || gshort_index == ':') {
			res = 1;
			break;
		}
		/* find the respective arg_option */
		if (!gshort_index) {
			i = 0;
			list_for_each(opt, options.list) {
				if (i == glong_index)
					break;
				i++;
			}
		} else {
			list_for_each(opt, options.list)
				if (opt->short_name == gshort_index)
					break;
		}
		assert(opt);

		if (optarg) {
			switch (opt->type) {
			case ARG_NONE:
				assert(0);
				break;
			case ARG_INT:
				*opt->action.int_var = atoi(optarg);
				break;
			case ARG_CHAR:
				*opt->action.char_var = strdup(optarg);
				break;
			case ARG_CALLBACK:
				/* will be handled below */
				break;
			}
		}
		if (opt->type == ARG_CALLBACK) {
			res = opt->action.callback(optarg);
			if (res)
				break;
		}
	}

	free(gshort);
	free(glong);

	return res;
}

static int str_append(char *dest, const char *src, int size, int col)
{
	int len, i;

	if (!size)
		return 0;
	len = strlen(dest);
	dest += len;
	i = 0;
	while (i < size - 1 && len + i < col) {
		*dest++ = ' ';
		i++;
	}
	while (i < size - 1 && *src) {
		*dest++ = *src++;
		i++;
	}
	*dest = '\0';
	return i;
}

#define HELP_BUF_SIZE	512
void arg_get_help(arg_help_handler_t handler)
{
	struct arg_option *opt;
	char *buf;
	int size;

	buf = malloc(HELP_BUF_SIZE);
	if (!buf)
		return;
	list_for_each(opt, options.list) {
		*buf = '\0';
		size = HELP_BUF_SIZE;
		if (opt->short_name) {
			buf[0] = '-';
			buf[1] = opt->short_name;
			buf[2] = '\0';
			size -= 2;
			if (opt->has_arg && !opt->long_name) {
				if (opt->has_arg == 1)
					size -= str_append(buf, " ARG", size, 0);
				else
					size -= str_append(buf, " [ARG]", size, 0);
			}
		}
		if (opt->long_name) {
			if (opt->short_name)
				size -= str_append(buf, ", ", size, 0);
			size -= str_append(buf, "--", size, 4);
			size -= str_append(buf, opt->long_name, 16, 0);
			if (opt->has_arg == 1)
				size -= str_append(buf, "=ARG", size, 0);
			else if (opt->has_arg == 2)
				size -= str_append(buf, "[=ARG]", size, 0);
		}
		size -= str_append(buf, opt->help, size, 26);
		handler(buf);
	}
	free(buf);
}
