#include <limits.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include "if.h"
#include "netns.h"
#include "utils.h"

struct generic_list {
	struct generic_list *next;
};

void list_free(void *list, destruct_f destruct)
{
	struct generic_list *cur = list, *next;

	while (cur) {
		next = cur->next;
		if (destruct)
			destruct(cur);
		free(cur);
		cur = next;
	}
}

char *ifstr(struct if_entry *entry)
{
	static char buf[IFNAMSIZ + NAME_MAX + 2];

	if (!entry->ns->name)
		/* root ns */
		snprintf(buf, sizeof(buf), "/%s", entry->if_name);
	else
		snprintf(buf, sizeof(buf), "%s/%s", entry->ns->name, entry->if_name);
	return buf;
}
