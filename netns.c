#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include "handler.h"
#include "if.h"
#include "utils.h"
#include "netns.h"

#define NETNS_RUN_DIR "/var/run/netns"

static int netns_get_list(struct netns_entry **result)
{
	struct netns_entry *entry, *ptr;
	struct dirent *de;
	DIR *dir;

	*result = ptr = calloc(sizeof(struct netns_entry), 1);
	dir = opendir(NETNS_RUN_DIR);
	if (!dir)
		return 0;

	while ((de = readdir(dir)) != NULL) {
		if (!strcmp(de->d_name, ".") ||
		    !strcmp(de->d_name, ".."))
			continue;
		entry = calloc(sizeof(struct netns_entry), 1);
		if (!entry)
			return ENOMEM;
		entry->name = strdup(de->d_name);
		if (!entry->name)
			return ENOMEM;
		ptr->next = entry;
		ptr = entry;
	}
	closedir(dir);

	return 0;
}

int netns_list(struct netns_entry **result)
{
	struct netns_entry *entry;
	int err;

	if ((err = netns_get_list(result)))
		return err;
	for (entry = *result; entry; entry = entry->next) {
		if (entry->name)
			if ((err = netns_switch(entry)))
				return err;
		if ((err = if_list(&entry->ifaces)))
			return err;
	}
	if ((err = handler_post(*result)))
		return err;
	return 0;
}

int netns_switch(struct netns_entry *dest)
{
	char net_path[PATH_MAX];
	int netns;

	snprintf(net_path, sizeof(net_path), "%s/%s", NETNS_RUN_DIR, dest->name);
	netns = open(net_path, O_RDONLY);
	if (netns < 0)
		return errno;
	if (setns(netns, CLONE_NEWNET) < 0)
		return errno;
	close(netns);
	return 0;
}

static void netns_list_destruct(struct netns_entry *entry)
{
	if_list_free(entry->ifaces);
	free(entry->name);
}

void netns_list_free(struct netns_entry *list)
{
	list_free(list, (destruct_f)netns_list_destruct);
}
