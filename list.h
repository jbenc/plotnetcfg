/*
 * This file is a part of plotnetcfg, a tool to visualize network config.
 * Copyright (C) 2016 Red Hat, Inc. -- Ondrej Hlavaty <ohlavaty@redhat.com>
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

#ifndef _LIST_H
#define _LIST_H

#include <stdlib.h>

/*
 * Provided for compatibility, until all code is transformed to lists.
 */
typedef void (*destruct_f)(void *);

static inline void slist_free(void *list, destruct_f destruct)
{
	struct generic_list {
		struct generic_list *next;
	} *cur, *next = list;

	while ((cur = next)) {
		next = cur->next;
		if (destruct)
			destruct(cur);
		free(cur);
	}
}

#endif
