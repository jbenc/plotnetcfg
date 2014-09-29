#ifndef _LABEL_H
#define _LABEL_H

struct label {
	struct label *next;
	char *text;
};

int label_add(struct label **list, char *fmt, ...);
void label_free(struct label *list);

#endif
