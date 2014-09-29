#ifndef _UTILS_H
#define _UTILS_H

struct if_entry;

#define _unused __attribute__((unused))

typedef void (*destruct_f)(void *);
void list_free(void *list, destruct_f destruct);

/* Returns static buffer. */
char *ifstr(struct if_entry *entry);
char *ifid(struct if_entry *entry);

/* dst has to be allocated, at least 16 bytes. Returns the family or -1. */
int raw_addr(void *dst, const char *src);

#endif
