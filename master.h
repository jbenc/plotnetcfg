/*
 * This file is a part of plotnetcfg, a tool to visualize network config.
 * Copyright (C) 2016 Red Hat, Inc. -- Jiri Benc <jbenc@redhat.com>,
 *                                     Ondrej Hlavaty <ohlavaty@redhat.com>
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

#ifndef _MASTER_H
#define _MASTER_H

struct if_entry;
struct netns_entry;

/*
 * Resolves entry->master_index to entry->master.
 */
int master_resolve(struct netns_entry *root);

/*
 * Updates slave->master and master->rev_master appropriately.
 */
int master_set(struct if_entry *master, struct if_entry *slave);

/*
 * Sets entry->link to link and adds link->rev_link entry of entry
 */
int link_set(struct if_entry *link, struct if_entry *entry);

/*
 * Sets both entries peer to each other. Previous peers will have their peer set to NULL.
 */
int peer_set(struct if_entry *link, struct if_entry *entry);

#endif
