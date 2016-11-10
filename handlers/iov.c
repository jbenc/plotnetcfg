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

#include "iov.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../handler.h"
#include "../if.h"
#include "../match.h"
#include "../sysfs.h"

struct netns_entry;

static int iov_scan(struct if_entry *entry);
static int iov_post(struct if_entry *entry, struct netns_entry *root);
static void iov_cleanup(struct if_entry *entry);

static struct handler h_iov = {
	.scan = iov_scan,
	.post = iov_post,
	.cleanup = iov_cleanup,
};

void handler_iov_register(void)
{
	handler_register(&h_iov);
}

static int iov_scan(struct if_entry *entry)
{
	char *resolved;
	char *path;

	if (asprintf(&path, "class/net/%s/device", entry->if_name) < 0)
		return errno;

	resolved = sysfs_realpath(path);
	free(path);

	if (resolved) {
		entry->pci_path = resolved;
	} else {
		if (errno == ENOENT)
			return 0; /* this is not a PCI device */
		return errno;
	}

	if (asprintf(&path, "class/net/%s/device/physfn", entry->if_name) < 0)
		return errno;

	resolved = sysfs_realpath(path);
	free(path);

	if (resolved) {
		entry->pci_physfn_path = resolved;
	} else {
		if (errno == ENOENT)
			return 0; /* this is not a VF */
		return errno;
	}

	return 0;
}

static int match_physfn(struct if_entry *physfn, void *arg)
{
	struct if_entry *entry = arg;

	if (!physfn->pci_path)
		return 0;

	if (!strcmp(physfn->pci_path, entry->pci_physfn_path))
		return 1;

	return 0;
}

static int iov_post(struct if_entry *entry, struct netns_entry *root)
{
	int err = 0;

	if (entry->physfn || !entry->pci_physfn_path)
		return 0;
	err = match_if(&entry->physfn, root, 1, entry, match_physfn, entry);
	if (err > 0)
		return err;
	if (!entry->physfn)
		return if_add_warning(entry, "failed to find the iov physfn");
	return 0;
}

static void iov_cleanup(struct if_entry *entry)
{
	if (entry->pci_path)
		sysfs_free(entry->pci_path);

	if (entry->pci_physfn_path)
		sysfs_free(entry->pci_physfn_path);
}
