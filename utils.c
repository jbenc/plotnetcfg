#include <stdlib.h>
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
