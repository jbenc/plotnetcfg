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
#include "utils.h"

/*
 * Simply insert the node as a member of a structure to insert it into lists.
 */
struct node {
	struct node *next, *prev;
};

/*
 * List is made from two virtual nodes in between all the other nodes are
 * inserted. This makes all corner cases base cases at the cost of two
 * always-NULL pointers extra.
 */
struct list {
	struct node head, tail;
};

#define node(n)			((struct node *) (n))
#define node_next(n)		((void *)((node(n))->next))
#define node_valid(n)		((node(n))->next)

#define list_head(list)		((void *)((list).head.next))
#define list_tail(list)		((void *)((list).tail.prev))
#define list_empty(list)	(!(list).head.next->next)

#define LIST_INITIALIZER(var)	{ .head = { &(var).tail, NULL }, .tail = { NULL,  &(var).head } }
#define DECLARE_LIST(var)	struct list var = LIST_INITIALIZER(var)

/*
 * When 'struct node' is embedded at the beginning of a structure,
 * the following macro can be used to iterate over the list.
 *
 * The node() macro allows easy access to the embedded node.
 */
#define list_for_each(n, list)	for (n = list_head(list); node_valid(n); n = node_next(n))

/*
 * When its not possible to make the node first, lists can still be used and
 * iterated over, but the following macros must be used.
 */
#define NODE_CONTAINER(ptr, type, member) \
	((type *) SKIP_BACK(type, member, ptr))

#define list_for_each_member(n, list, member)					\
	for ((n) = NODE_CONTAINER(list_head(list), __typeof__(*n), member);	\
	    node_valid(&(n)->member);						\
	    n = NODE_CONTAINER(node_next(&(n)->member), __typeof__(*n), member))

static inline void list_init(struct list *l)
{
	l->head.next = &l->tail;
	l->tail.prev = &l->head;
	l->head.prev = l->tail.next = NULL;
}

/*
 *  *-------*               *-----*
 *  | after | \           / |  z  |
 *  *-------*  \ *-----* /  *-----*
 *               |  n  |
 *               *-----*
 */
static inline void list_insert_after(struct node *after, struct node *n)
{
	struct node *z = after->next;

	after->next = n;
	n->prev = after;
	n->next = z;
	z->prev = n;
}

/*
 *  *-----*               *--------*
 *  |  a  | \           / | before |
 *  *-----*  \ *-----* /  *--------*
 *             |  n  |
 *             *-----*
 */
static inline void list_insert_before(struct node *before, struct node *n)
{
	struct node *a = before->prev;

	before->prev = n;
	n->next = before;
	n->prev = a;
	a->next = n;
}

static inline void list_append(struct list *l, struct node *n)
{
	list_insert_before(&l->tail, n);
}

static inline void list_prepend(struct list *l, struct node *n)
{
	list_insert_after(&l->head, n);
}

/*
 * returns void * to inhibit type-checking
 */
static inline void *node_remove(struct node *n)
{
	struct node *z = n->prev;
	struct node *a = n->next;

	z->next = a;
	a->prev = z;
	n->next = NULL;
	n->prev = NULL;

	return n;
}

static inline void *list_pop(struct list *l)
{
	return list_empty(*l) ? NULL : node_remove(list_tail(*l));
}

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

static inline void list_free(void *list, destruct_f destruct)
{
	struct node *n;

	while ((n = list_pop(list))) {
		if (destruct)
			destruct(n);
		free(n);
	}
}

#endif
