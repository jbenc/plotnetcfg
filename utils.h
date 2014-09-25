#ifndef _UTILS_H
#define _UTILS_H

struct if_entry;

#define _unused __attribute__((unused))

typedef void (*destruct_f)(void *);
void list_free(void *list, destruct_f destruct);

/* Returns static buffer. */
char *ifstr(struct if_entry *entry);
char *ifdot(struct if_entry *entry);

#endif
