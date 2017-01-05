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

#ifndef _ARGS_H
#define _ARGS_H

#include "list.h"

enum arg_type {
	ARG_NONE,
	ARG_INT,
	ARG_CHAR,
	ARG_CALLBACK,
};

union arg_action {
	void *void_var;
	int *int_var;
	char **char_var;
	/* callback returns 0 in case of success, >0 in case of error */
	int (*callback)(char *arg);
};

struct arg_option {
	struct node n;
	const char *long_name;
	char short_name;
	int has_arg;	/* see getopt(3) for possible values */
	enum arg_type type;
	union arg_action action;
	const char *help;
};

void arg_register(struct arg_option *opt);
void arg_register_batch(struct arg_option *opt, int count);
int arg_parse(int argc, char **argv);

typedef void (*arg_help_handler_t)(const char *help);

void arg_get_help(arg_help_handler_t handler);

#endif
