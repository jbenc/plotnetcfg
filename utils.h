#ifndef _UTILS_H
#define _UTILS_H

#define _unused __attribute__((unused))

typedef void (*destruct_f)(void *);

void list_free(void *list, destruct_f destruct);

#endif
