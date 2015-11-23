/*
 * This file is a part of plotnetcfg, a tool to visualize network config.
 * Copyright (C) 2015 Red Hat, Inc. -- Ondrej Hlavaty <ohlavaty@redhat.com>
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../sysfs.h"
#include "../label.h"
#include "../handler.h"
#include "../match.h"
#include "iov.h"

static int iov_scan(struct if_entry *entry);
static int iov_post(struct if_entry *entry, struct netns_entry *root);

static struct handler h_iov = {
	.scan = iov_scan,
	.post = iov_post,
};

void handler_iov_register(void)
{
	handler_register(&h_iov);
}

static int iov_scan(struct if_entry *entry)
{
	char *resolved;
	static char *pathbuf = NULL;

	if (pathbuf == NULL) {
		if ((pathbuf = malloc(sysfs_path_max)) == NULL)
			return errno;
	}

	snprintf(pathbuf, sysfs_path_max, "class/net/%s/device", entry->if_name);
	if ((resolved = sysfs_realpath(pathbuf)) != NULL) {
		entry->pci_path = resolved;
	} else {
		if (errno != ENOENT) // Ignore if not a PCI device
			return errno;
	}

	snprintf(pathbuf, sysfs_path_max, "class/net/%s/device/physfn", entry->if_name);
	if ((resolved = sysfs_realpath(pathbuf)) != NULL) {
		entry->pci_physfn = resolved;
	} else {
		if (errno != ENOENT) // Ignore if not a VF
			return errno;
	}
	return 0;
}

static int match_physfn(struct if_entry *physfn, void *arg)
{
	struct if_entry *entry = arg;

	if (!physfn->pci_path)
		return 0;

	if (!strcmp(physfn->pci_path, entry->pci_physfn))
		return 1;

	return 0;
}

static int iov_post(struct if_entry *entry, struct netns_entry *root)
{
	int err = 0;

	if (entry->physfn || !entry->pci_physfn)
		return 0;
	err = match_if_heur(&entry->physfn, root, 1, entry, match_physfn, entry);
	if (err > 0)
		return err;
	if (err < 0)
		return if_add_warning(entry, "failed to find the iov physfn reliably");
	if (!entry->physfn)
		return if_add_warning(entry, "failed to find the iov physfn");
	return 0;
}
