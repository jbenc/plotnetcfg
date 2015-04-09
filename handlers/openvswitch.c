/*
 * This file is a part of plotnetcfg, a tool to visualize network config.
 * Copyright (C) 2014 Red Hat, Inc. -- Jiri Benc <jbenc@redhat.com>
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

#define _GNU_SOURCE
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/un.h>
#include <net/if.h>
#include <unistd.h>
#include "../args.h"
#include "../handler.h"
#include "../if.h"
#include "../label.h"
#include "../match.h"
#include "../netns.h"
#include "../tunnel.h"
#include "../utils.h"
#include "../parson/parson.h"
#include "openvswitch.h"

#define OVS_DB_DEFAULT	"/var/run/openvswitch/db.sock";
static char *db;

struct ovs_if {
	struct ovs_if *next;
	struct ovs_port *port;
	struct if_entry *link;
	char *name;
	char *type;
	/* for vxlan: */
	char *local_ip;
	char *remote_ip;
	/* for patch port: */
	char *peer;
};

struct ovs_port {
	struct ovs_port *next;
	struct ovs_bridge *bridge;
	struct if_entry *link;
	char *name;
	struct ovs_if *ifaces;
	int iface_count;
	/* vlan tags: */
	unsigned int tag;
	unsigned int trunks_count;
	unsigned int *trunks;
	/* bonding: */
	char *bond_mode;
};

struct ovs_bridge {
	struct ovs_bridge *next;
	char *name;
	struct ovs_port *ports;
	struct ovs_port *system;
};

static struct ovs_bridge *br_list;

static int is_set(JSON_Array *j)
{
	return (!strcmp(json_array_get_string(j, 0), "set"));
}

static int is_map(JSON_Array *j)
{
	return (!strcmp(json_array_get_string(j, 0), "map"));
}

static int is_uuid(JSON_Array *j)
{
	return (!strcmp(json_array_get_string(j, 0), "uuid"));
}

static int is_empty(JSON_Value *j)
{
	return json_type(j) == JSONArray && is_set(json_array(j));
}

static struct ovs_if *parse_iface(JSON_Object *jresult, JSON_Array *uuid)
{
	struct ovs_if *iface;
	JSON_Object *jif;
	JSON_Array *jarr;
	unsigned int i;

	if (!is_uuid(uuid))
		return NULL;
	jif = json_object_get_object(jresult, "Interface");
	jif = json_object_get_object(jif, json_array_get_string(uuid, 1));
	jif = json_object_get_object(jif, "new");

	iface = calloc(sizeof(*iface), 1);
	if (!iface)
		return NULL;
	iface->name = strdup(json_object_get_string(jif, "name"));
	iface->type = strdup(json_object_get_string(jif, "type"));
	jarr = json_object_get_array(jif, "options");
	if (!strcmp(iface->type, "vxlan") && is_map(jarr)) {
		jarr = json_array_get_array(jarr, 1);
		for (i = 0; i < json_array_get_count(jarr); i++) {
			JSON_Array *jkv;
			const char *key, *val;

			jkv = json_array_get_array(jarr, i);
			key = json_array_get_string(jkv, 0);
			val = json_array_get_string(jkv, 1);
			if (!strcmp(key, "local_ip"))
				iface->local_ip = strdup(val);
			else if (!strcmp(key, "remote_ip"))
				iface->remote_ip = strdup(val);
		}
	} else if (!strcmp(iface->type, "patch") && is_map(jarr)) {
		jarr = json_array_get_array(jarr, 1);
		for (i = 0; i < json_array_get_count(jarr); i++) {
			JSON_Array *jkv;
			const char *key, *val;

			jkv = json_array_get_array(jarr, i);
			key = json_array_get_string(jkv, 0);
			val = json_array_get_string(jkv, 1);
			if (!strcmp(key, "peer"))
				iface->peer = strdup(val);
		}
	}
	return iface;
}

static int parse_port_info(struct ovs_port *port, JSON_Object *jport)
{
	JSON_Value *jval;
	JSON_Array *jarr;
	unsigned int i, cnt;

	jval = json_object_get_value(jport, "tag");
	if (!is_empty(jval))
		port->tag = json_number(jval);
	jarr = json_object_get_array(jport, "trunks");
	jarr = json_array_get_array(jarr, 1);
	cnt = json_array_get_count(jarr);
	if (cnt > 0) {
		port->trunks = malloc(sizeof(*port->trunks) * cnt);
		if (!port->trunks)
			return ENOMEM;
		port->trunks_count = cnt;
		for (i = 0; i < cnt; i++)
			port->trunks[i] = json_array_get_number(jarr, i);
	}

	jval = json_object_get_value(jport, "bond_mode");
	if (!is_empty(jval))
		port->bond_mode = strdup(json_string(jval));
	return 0;
}

static struct ovs_port *parse_port(JSON_Object *jresult, JSON_Array *uuid,
				   struct ovs_bridge *br)
{
	struct ovs_port *port;
	struct ovs_if *ptr, *iface;
	JSON_Object *jport;
	JSON_Array *jarr;
	unsigned int i;

	if (!is_uuid(uuid))
		return NULL;
	jport = json_object_get_object(jresult, "Port");
	jport = json_object_get_object(jport, json_array_get_string(uuid, 1));
	jport = json_object_get_object(jport, "new");

	port = calloc(sizeof(*port), 1);
	if (!port)
		return NULL;
	port->name = strdup(json_object_get_string(jport, "name"));
	port->bridge = br;
	if (parse_port_info(port, jport))
		return NULL;
	jarr = json_object_get_array(jport, "interfaces");
	if (is_set(jarr)) {
		jarr = json_array_get_array(jarr, 1);
		for (i = 0; i < json_array_get_count(jarr); i++) {
			iface = parse_iface(jresult, json_array_get_array(jarr, i));
			if (!iface)
				return NULL;
			iface->port = port;
			if (!port->ifaces)
				port->ifaces = iface;
			else
				ptr->next = iface;
			ptr = iface;
			port->iface_count++;
		}
	} else {
		iface = parse_iface(jresult, jarr);
		if (!iface)
			return NULL;
		iface->port = port;
		port->ifaces = iface;
		port->iface_count = 1;
	}

	if (!strcmp(json_object_get_string(jport, "name"), br->name))
		br->system = port;

	return port;
}

static struct ovs_bridge *parse_bridge(JSON_Object *jresult, JSON_Array *uuid)
{
	struct ovs_bridge *br;
	struct ovs_port *ptr, *port;
	JSON_Object *jbridge;
	JSON_Array *jarr;
	unsigned int i;

	if (!is_uuid(uuid))
		return NULL;
	jbridge = json_object_get_object(jresult, "Bridge");
	jbridge = json_object_get_object(jbridge, json_array_get_string(uuid, 1));
	jbridge = json_object_get_object(jbridge, "new");

	br = calloc(sizeof(*br), 1);
	if (!br)
		return NULL;
	br->name = strdup(json_object_get_string(jbridge, "name"));
	jarr = json_object_get_array(jbridge, "ports");
	if (is_set(jarr)) {
		jarr = json_array_get_array(jarr, 1);
		for (i = 0; i < json_array_get_count(jarr); i++) {
			port = parse_port(jresult, json_array_get_array(jarr, i), br);
			if (!port)
				return NULL;
			if (!br->ports)
				br->ports = port;
			else
				ptr->next = port;
			ptr = port;
		}
	} else
		br->ports = parse_port(jresult, jarr, br);
	if (!br->ports)
		return NULL;
	return br;
}

static struct ovs_bridge *parse(char *answer)
{
	struct ovs_bridge *list = NULL, *ptr, *br;
	JSON_Value *jroot;
	JSON_Object *jresult, *jovs;
	JSON_Array *jarr;
	unsigned int i;

	jroot = json_parse_string(answer);
	if (!jroot)
		return NULL;
	jresult = json_object_get_object(json_object(jroot), "result");
	if (!jresult)
		return NULL;
	/* TODO: add the rest of error handling */
	jovs = json_object_get_object(jresult, "Open_vSwitch");
	if (json_object_get_count(jovs) != 1)
		return NULL;
	jovs = json_object_get_object(jovs, json_object_get_name(jovs, 0));
	jovs = json_object_get_object(jovs, "new");

	jarr = json_object_get_array(jovs, "bridges");
	if (is_set(jarr)) {
		jarr = json_array_get_array(jarr, 1);
		for (i = 0; i < json_array_get_count(jarr); i++) {
			br = parse_bridge(jresult, json_array_get_array(jarr, i));
			if (!br)
				return NULL;
			if (!list)
				list = br;
			else
				ptr->next = br;
			ptr = br;
		}
	} else
		list = parse_bridge(jresult, jarr);
	return list;
}


static void add_table(JSON_Object *parmobj, char *table, ...)
{
	va_list ap;
	JSON_Value *new;
	JSON_Object *tableobj;
	JSON_Array *cols;
	char *s;

	va_start(ap, table);
	new = json_value_init_object();
	tableobj = json_object(new);
	json_object_set(parmobj, table, new);
	new = json_value_init_array();
	cols = json_array(new);
	json_object_set(tableobj, "columns", new);
	while ((s = va_arg(ap, char *)))
		json_array_append(cols, json_value_init_string(s));
	va_end(ap);
}

static char *construct_query(void)
{
	JSON_Value *root, *new;
	JSON_Object *ro, *po;
	JSON_Array *params;
	char *res;

	root = json_value_init_object();
	ro = json_object(root);
	json_object_set(ro, "method", json_value_init_string("monitor"));
	json_object_set(ro, "id", json_value_init_number(0));

	new = json_value_init_array();
	params = json_array(new);
	json_object_set(ro, "params", new);
	json_array_append(params, json_value_init_string("Open_vSwitch"));
	json_array_append(params, json_value_init_null());
	new = json_value_init_object();
	po = json_object(new);
	json_array_append(params, new);
	add_table(po, "Open_vSwitch", "bridges", "ovs_version", NULL);
	add_table(po, "Bridge", "name", "ports", NULL);
	add_table(po, "Port", "interfaces", "name", "tag", "trunks", "bond_mode", NULL);
	add_table(po, "Interface", "name", "type", "options", "admin_state", "link_state", NULL);

	res = json_serialize(root);
	json_value_free(root);
	return res;
}

static int connect_ovs(void)
{
	int fd;
	struct sockaddr_un sun;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		return fd;
	sun.sun_family = AF_UNIX;
	strncpy(sun.sun_path, db, UNIX_PATH_MAX);
	sun.sun_path[UNIX_PATH_MAX - 1] = '\0';
	connect(fd, (struct sockaddr *)&sun, sizeof(sun.sun_family) + strlen(sun.sun_path) + 1);
	return fd;
}

#define CHUNK	65536
static char *read_all(int fd)
{
	char *buf, *newbuf;
	size_t len = 0;
	ssize_t res;

	buf = malloc(CHUNK);
	if (!buf)
		return NULL;
	while (1) {
		res = read(fd, buf + len, CHUNK);
		if (res < 0) {
			free(buf);
			return NULL;
		}
		len += res;
		if (res < CHUNK) {
			buf[len] = '\0';
			return buf;
		}
		newbuf = realloc(buf, len + CHUNK);
		if (!newbuf) {
			free(buf);
			return NULL;
		}
		buf = newbuf;
	}
}

static int link_iface_search(struct if_entry *entry, void *arg)
{
	struct ovs_if *iface = arg;
	int search_for_system = !iface->port->bridge->system->link;
	int weight;

	if (!search_for_system &&
	    entry->master && strcmp(entry->master->if_name, "ovs-system"))
		return 0;
	/* Ignore ifindex reported by ovsdb, as it is guessed by the
	 * interface name anyway and does not work correctly accross netns.
	 * The heuristics below is much more reliable, though obviously far
	 * from good, it fails spectacularly when the netdev interface is
	 * renamed.
	 */
	if (strcmp(iface->name, entry->if_name))
		return 0;
	if (!strcmp(iface->type, "internal") &&
	    strcmp(entry->driver, "openvswitch"))
		return 0;
	weight = 1;
	if (!search_for_system) {
		if (iface->port->bridge->system->link->ns == entry->ns)
			weight++;
	} else {
		if (!entry->ns->name)
			weight++;
	}
	return weight;
}

static int link_iface(struct ovs_if *iface, struct netns_entry *root, int required)
{
	int err;

	if (iface->link)
		return 0;
	err = match_if_heur(&iface->link, root, 1, NULL, link_iface_search, iface);
	if (err > 0)
		return err;
	if (err < 0)
		return label_add(&root->warnings,
				 "Failed to map openvswitch interface %s reliably",
				 iface->name);
	if (required && !iface->link)
		return label_add(&root->warnings,
				 "Failed to map openvswitch interface %s",
				 iface->name);
	return 0;
}

static struct if_entry *create_iface(char *name, char *br_name, struct netns_entry *root)
{
	struct if_entry *entry;
	char buf[IFNAMSIZ + 4 + 1];

	entry = calloc(sizeof(*entry), 1);
	if (!entry)
		return NULL;
	entry->ns = root;
	snprintf(buf, sizeof(buf), "ovs:%s", br_name);
	entry->internal_ns = strdup(buf);
	entry->if_name = strdup(name);
	if (!entry->internal_ns || !entry->if_name)
		return NULL;
	entry->flags |= IF_INTERNAL;

	if_append(&root->ifaces, entry);
	return entry;
}

static void label_iface(struct ovs_if *iface)
{
	if (iface->type && *iface->type)
		label_add(&iface->link->label, "type: %s", iface->type);
	if (iface->local_ip)
		label_add(&iface->link->label, "from %s", iface->local_ip);
	if (iface->remote_ip)
		label_add(&iface->link->label, "to %s", iface->remote_ip);
}

static void label_port_or_iface(struct ovs_port *port, struct if_entry *link)
{
	if (port->tag)
		 asprintf(&link->edge_label, "tag %u", port->tag);
	else if (port->trunks_count) {
		char *buf, *ptr;
		unsigned int i;

		buf = malloc(16 * port->trunks_count + 7 + 1);
		if (!buf)
			return;
		ptr = buf + sprintf(buf, "trunks %u", port->trunks[0]);
		for (i = 1; i < port->trunks_count; i++)
			ptr += sprintf(ptr, ", %u", port->trunks[i]);
		link->edge_label = buf;
	}
	if (port->bond_mode)
		label_add(&link->label, "bond mode: %s", port->bond_mode);
}

static void link_vxlan(struct ovs_if *iface)
{
	if (!iface->local_ip || !*iface->local_ip)
		return;
	iface->link->peer = tunnel_find_iface(iface->link->ns, iface->local_ip);
}

static int link_patch_search(struct if_entry *entry, void *arg)
{
	struct ovs_if *iface = arg;

	if (!iface->peer ||
	    strcmp(iface->peer, entry->if_name) ||
	    !(entry->flags & IF_INTERNAL))
		return 0;
	return 1;
}

static int link_patch(struct ovs_if *iface, struct netns_entry *root)
{
	int err;

	err = match_if_heur(&iface->link->peer, root, 1, NULL, link_patch_search, iface);
	if (err > 0)
		return err;
	if (err < 0)
		return if_add_warning(iface->link, "failed to find openvswitch patch port peer reliably");
	if (iface->link->peer)
		iface->link->peer->peer = iface->link;
	/* Ignore case when the peer is not found, it will be found from the
	 * other side. */
	return 0;
}

static int link_ifaces(struct netns_entry *root)
{
	struct ovs_bridge *br;
	struct ovs_port *port;
	struct ovs_if *iface;
	struct if_entry *master;
	int err;

	for (br = br_list; br; br = br->next) {
		if (!br->system || !br->system->iface_count)
			return label_add(&root->warnings,
					 "Failed to find main interface for openvswitch bridge %s",
					 br->name);
		if (br->system->iface_count > 1)
			return label_add(&root->warnings,
					 "Main port for openvswitch bridge %s appears to have several interfaces",
					 br->name);
		if ((err = link_iface(br->system->ifaces, root, 1)))
			return err;
		for (port = br->ports; port; port = port->next) {
			if (port == br->system)
				continue;
			master = br->system->ifaces->link;
			if (port->iface_count > 1) {
				port->link = create_iface(port->name, port->bridge->name, root);
				if (!port->link)
					return ENOMEM;
				port->link->master = master;
				master = port->link;
				label_port_or_iface(port, port->link);
			}
			for (iface = port->ifaces; iface; iface = iface->next) {
				if ((err = link_iface(iface, root, 0)))
					return err;
				if (!iface->link) {
					iface->link = create_iface(iface->name,
								   iface->port->bridge->name,
								   root);
					if (!iface->link)
						return ENOMEM;
				}

				/* reconnect to the ovs master */
				iface->link->master = master;

				label_iface(iface);
				if (port->iface_count == 1)
					label_port_or_iface(port, iface->link);
				if (!strcmp(iface->type, "vxlan"))
					link_vxlan(iface);
				else if (!strcmp(iface->type, "patch")) {
					if ((err = link_patch(iface, root)))
						return err;
				}
			}
		}
	}
	return 0;
}

static int ovs_global_post(struct netns_entry *root)
{
	char *str;
	int fd, len;
	int err;

	str = construct_query();
	len = strlen(str);
	fd = connect_ovs();
	if (fd < 0)
		return 0;
	if (write(fd, str, len) < len) {
		close(fd);
		return 0;
	}
	json_free_serialization_string(str);
	str = read_all(fd);
	br_list = parse(str);
	free(str);
	close(fd);
	if (!br_list)
		return 0;
	if ((err = link_ifaces(root)))
		return err;
	return 0;
}

static void destruct_if(struct ovs_if *iface)
{
	free(iface->name);
	free(iface->type);
	free(iface->local_ip);
	free(iface->remote_ip);
}

static void destruct_port(struct ovs_port *port)
{
	free(port->name);
	free(port->trunks);
	list_free(port->ifaces, (destruct_f)destruct_if);
}

static void destruct_bridge(struct ovs_bridge *br)
{
	free(br->name);
	list_free(br->ports, (destruct_f)destruct_port);
}

static void ovs_global_cleanup(_unused struct netns_entry *root)
{
	list_free(br_list, (destruct_f)destruct_bridge);
}

static struct global_handler gh_ovs = {
	.post = ovs_global_post,
	.cleanup = ovs_global_cleanup,
};

static struct arg_option options[] = {
	{ .long_name = "ovs-db", .short_name = 'D', .has_arg = 1,
	  .type = ARG_CHAR, .action.char_var = &db,
	  .help = "path to openvswitch database" },
};

void handler_ovs_register(void)
{
	db = OVS_DB_DEFAULT;
	arg_register_batch(options, ARRAY_SIZE(options));
	global_handler_register(&gh_ovs);
}
