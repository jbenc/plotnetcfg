#define _GNU_SOURCE
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "label.h"

int label_add(struct label **list, char *fmt, ...)
{
	va_list ap;
	struct label *new, *ptr = *list;

	va_start(ap, fmt);
	new = calloc(sizeof(*new), 1);
	if (!new)
		return ENOMEM;
	if (vasprintf(&new->text, fmt, ap) < 0) {
		free(new);
		return ENOMEM;
	}

	if (!ptr) {
		*list = new;
		return 0;
	}
	while (ptr->next)
		ptr = ptr->next;
	ptr->next = new;
	return 0;
}

static void label_destruct(struct label *item)
{
	free(item->text);
}

void label_free(struct label *list)
{
	list_free(list, (destruct_f)label_destruct);
}
