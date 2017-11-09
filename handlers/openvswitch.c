/*
 * This file is a part of plotnetcfg, a tool to visualize network config.
 * Copyright (C) 2014 Red Hat, Inc. -- Jiri Benc <jbenc@redhat.com>
 * Copyright (C) 2017 Red Hat, Inc. -- Ondrej Hlavaty <ohlavaty@redhat.com>
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

#include "openvswitch.h"
#include <errno.h>
#include <error.h>
#include <jansson.h>
#include <net/if.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include "../args.h"
#include "../handler.h"
#include "../if.h"
#include "../label.h"
#include "../list.h"
#include "../master.h"
#include "../match.h"
#include "../netlink.h"
#include "../netns.h"
#include "../tunnel.h"
#include "../utils.h"

#include "../compat.h"

#define OVS_DB_DEFAULT	"/var/run/openvswitch/db.sock";
static char *db;
static unsigned int vport_genl_id;

#define OVS_WARN "Failed to handle openvswitch: "

struct ovs_if {
	struct node n;
	struct ovs_port *port;
	struct if_entry *link;
	char *name;
	char *type;
	/* for tunnels: */
	char *local_ip;
	char *remote_ip;
	char *key;
	/* for patch port: */
	char *peer;
};

struct ovs_port {
	struct node n;
	struct ovs_bridge *bridge;
	struct if_entry *link;
	char *name;
	struct list ifaces;
	int iface_count;
	/* vlan tags: */
	unsigned int tag;
	unsigned int trunks_count;
	unsigned int *trunks;
	/* bonding: */
	char *bond_mode;
};

struct ovs_bridge {
	struct node n;
	char *name;
	struct list ports;
	struct ovs_port *system;
};

static DECLARE_LIST(br_list);

static int is_set(json_t *j)
{
	return (!strcmp(json_string_value(json_array_get(j, 0)), "set"));
}

static int is_map(json_t *j)
{
	return (!strcmp(json_string_value(json_array_get(j, 0)), "map"));
}

static int is_uuid(json_t *j)
{
	return (!strcmp(json_string_value(json_array_get(j, 0)), "uuid"));
}

static int is_empty(json_t *j)
{
	return json_is_array(j) && is_set(j);
}

static int search_str_option(char **dest, json_t *jarr, const char *search_name)
{
	unsigned int i;

	*dest = NULL;

	for (i = 0; i < json_array_size(jarr); i++) {
		json_t *jkv = json_array_get(jarr, i);
		const char *key = json_string_value(json_array_get(jkv, 0));
		if (!strcmp(key, search_name)) {
			if (!(*dest = strdup(json_string_value(json_array_get(jkv, 1)))))
				return ENOMEM;
			return 0;
		}
	}
	return 0;
}

static int iface_is_tunnel(const struct ovs_if *iface)
{
	return !strcmp(iface->type, "vxlan")
		|| !strcmp(iface->type, "geneve")
		|| !strcmp(iface->type, "gre");
}

static void destruct_bridge(struct ovs_bridge *br);
static void destruct_port(struct ovs_port *port);
static void destruct_if(struct ovs_if *iface);

static int parse_iface(json_t *jresult, json_t *uuid, struct ovs_port *port, struct list *warnings)
{
	struct ovs_if *iface;
	json_t *jif, *jarr;
	const char *name, *type;
	int err;

	if (!is_uuid(uuid)) {
		label_add(warnings, OVS_WARN "expected interface uuid");
		return EINVAL;
	}

	jif = json_object_get(jresult, "Interface");
	jif = json_object_get(jif, json_string_value(json_array_get(uuid, 1)));
	jif = json_object_get(jif, "new");
	if (!jif) {
		label_add(warnings, OVS_WARN "interface structure broken");
		return EINVAL;
	}

	if (!(name = json_string_value(json_object_get(jif, "name")))) {
		label_add(warnings, OVS_WARN "expected interface name");
		return EINVAL;
	}

	if (!(type = json_string_value(json_object_get(jif, "type")))) {
		label_add(warnings, OVS_WARN "expected interface type");
		return EINVAL;
	}

	if (!(iface = calloc(1, sizeof(*iface))))
		return ENOMEM;

	if (!(iface->name = strdup(name))
	 || !(iface->type = strdup(type))) {
		err = ENOMEM;
		goto err_iface;
	}

	jarr = json_object_get(jif, "options");
	if (is_map(jarr)) {
		jarr = json_array_get(jarr, 1);
		if (iface_is_tunnel(iface)) {
			if ((err = search_str_option(&iface->local_ip, jarr, "local_ip"))
			 || (err = search_str_option(&iface->remote_ip, jarr, "remote_ip"))
			 || (err = search_str_option(&iface->key, jarr, "key")))
				goto err_iface;
		} else if (!strcmp(iface->type, "patch")) {
			if ((err = search_str_option(&iface->peer, jarr, "peer")))
				goto err_iface;
		}
	}

	iface->port = port;
	list_append(&port->ifaces, node(iface));
	port->iface_count++;

	return 0;

err_iface:
	destruct_if(iface);
	free(iface);
	return EINVAL;
}

static int parse_port_info(struct ovs_port *port, json_t *jport)
{
	json_t *jval, *jarr;
	unsigned int i, cnt;

	jval = json_object_get(jport, "tag");
	if (!is_empty(jval))
		port->tag = json_integer_value(jval);
	jarr = json_object_get(jport, "trunks");
	jarr = json_array_get(jarr, 1);
	cnt = json_array_size(jarr);
	if (cnt > 0) {
		port->trunks = malloc(sizeof(*port->trunks) * cnt);
		if (!port->trunks)
			return ENOMEM;
		port->trunks_count = cnt;
		for (i = 0; i < cnt; i++)
			port->trunks[i] = json_integer_value(json_array_get(jarr, i));
	}

	jval = json_object_get(jport, "bond_mode");
	if (!is_empty(jval)) {
		if (!(port->bond_mode = strdup(json_string_value(jval))))
			return ENOMEM;
	}

	return 0;
}

static int parse_port(json_t *jresult, json_t *uuid,
		      struct ovs_bridge *br, struct list *warnings)
{
	struct ovs_port *port;
	json_t *jport, *jarr;
	const char *name;
	unsigned int i;
	int err;

	if (!is_uuid(uuid)) {
		label_add(warnings, OVS_WARN "expected port uuid");
		return EINVAL;
	}

	jport = json_object_get(jresult, "Port");
	jport = json_object_get(jport, json_string_value(json_array_get(uuid, 1)));
	jport = json_object_get(jport, "new");
	if (!jport) {
		label_add(warnings, OVS_WARN "port structure broken");
		return EINVAL;
	}

	name = json_string_value(json_object_get(jport, "name"));
	if (!name) {
		label_add(warnings, OVS_WARN "expected port name");
		return EINVAL;
	}

	if (!(port = calloc(1, sizeof(*port))))
		return ENOMEM;

	list_init(&port->ifaces);
	port->bridge = br;

	if (!(port->name = strdup(name)))
		goto err_port;

	if ((err = parse_port_info(port, jport)))
		goto err_port;

	jarr = json_object_get(jport, "interfaces");
	if (is_set(jarr)) {
		jarr = json_array_get(jarr, 1);
		for (i = 0; i < json_array_size(jarr); i++) {
			if ((err = parse_iface(jresult, json_array_get(jarr, i), port, warnings)))
				goto err_port;
		}
	} else {
		if ((err = parse_iface(jresult, jarr, port, warnings)))
			goto err_port;
	}

	if (!strcmp(json_string_value(json_object_get(jport, "name")), br->name))
		br->system = port;
	else
		list_append(&br->ports, node(port));

	return 0;

err_port:
	destruct_port(port);
	free(port);
	return err;
}

static int parse_bridge(struct ovs_bridge **dest, json_t *jresult,
			json_t *uuid, struct list *warnings)
{
	struct ovs_bridge *br;
	json_t *jbridge, *jarr;
	const char *name;
	unsigned int i;
	int err;

	*dest = NULL;

	if (!is_uuid(uuid)) {
		label_add(warnings, OVS_WARN "expected bridge uuid");
		return EINVAL;
	}

	jbridge = json_object_get(jresult, "Bridge");
	jbridge = json_object_get(jbridge, json_string_value(json_array_get(uuid, 1)));
	jbridge = json_object_get(jbridge, "new");
	if (!jbridge) {
		label_add(warnings, OVS_WARN "bridge structure broken");
		return EINVAL;
	}

	name = json_string_value(json_object_get(jbridge, "name"));
	if (!name) {
		label_add(warnings, OVS_WARN "expected bridge name");
		return EINVAL;
	}

	if (!(br = calloc(1, sizeof(*br))))
		return ENOMEM;
	list_init(&br->ports);

	br->name = strdup(name);
	if (!br->name)
		goto err_br;

	jarr = json_object_get(jbridge, "ports");
	if (is_set(jarr)) {
		jarr = json_array_get(jarr, 1);
		for (i = 0; i < json_array_size(jarr); i++) {
			if ((err = parse_port(jresult, json_array_get(jarr, i), br, warnings)))
				goto err_br;
		}
	} else {
		if ((err = parse_port(jresult, jarr, br, warnings)))
			goto err_br;
	}

	*dest = br;
	return 0;

err_br:
	destruct_bridge(br);
	free(br);
	return err;
}

static int parse(struct list *br_list, char *answer, struct  list *warnings)
{
	struct ovs_bridge *br;
	json_t *jroot, *jresult, *jovs, *jarr, *jerror;
	json_error_t jerr;
	unsigned int i;
	int err = EINVAL;

	if (!(jroot = json_loads(answer, 0, &jerr))) {
		label_add(warnings,
		    OVS_WARN "cannot parse response: %s", jerr.text);
		return EINVAL;
	}

	if ((jerror = json_object_get(jroot, "error"))
	    && !json_is_null(jerror)) {
		jerror = json_object_get(jerror, "error");
		label_add(warnings,
		    OVS_WARN "server reported an error: %s", json_string_value(jerror));
		goto err_root;
	}

	if (!(jresult = json_object_get(jroot, "result"))
	 || !(jovs = json_object_get(jresult, "Open_vSwitch"))) {
		label_add(warnings,
		    OVS_WARN "unexpected structure of response");
		goto err_root;
	}

	if (json_object_size(jovs) != 1) {
		label_add(warnings,
		    OVS_WARN "unexpected number of rows in response");
		goto err_root;
	}

	jarr = json_object_iter_value(json_object_iter(jovs));
	jarr = json_object_get(jarr, "new");
	jarr = json_object_get(jarr, "bridges");
	if (!jarr) {
		label_add(warnings,
		    OVS_WARN "unexpected structure of response");
		goto err_root;
	}

	if (is_set(jarr)) {
		jarr = json_array_get(jarr, 1);
		for (i = 0; i < json_array_size(jarr); i++) {
			if ((err = parse_bridge(&br, jresult, json_array_get(jarr, i), warnings)))
				goto err_root;
			list_append(br_list, node(br));
		}
	} else {
		if ((err = parse_bridge(&br, jresult, jarr, warnings)))
			goto err_root;
		list_append(br_list, node(br));
	}

err_root:
	json_decref(jroot);
	return err;
}


static int add_table(json_t *parmobj, char *table, ...)
{
	va_list ap;
	json_t *tableobj, *cols;
	char *s;
	int err = ENOMEM;

	va_start(ap, table);

	if (!(tableobj = json_object()))
		goto err;

	if (!(cols = json_array()))
		goto err_table;

	while ((s = va_arg(ap, char *)))
		if ((err = json_array_append_new(cols, json_string(s))))
			goto err_cols;

	if ((err = json_object_set(tableobj, "columns", cols))
	 || (err = json_object_set(parmobj, table, tableobj)))
		goto err_cols;

err_cols:
	json_decref(cols);
err_table:
	json_decref(tableobj);
err:
	va_end(ap);
	return err;
}

static char *construct_query(void)
{
	json_t *root, *params, *po;
	char *res = NULL;

	if (!(po = json_object()))
		return NULL;

	if (add_table(po, "Open_vSwitch", "bridges", "ovs_version", NULL)
	 || add_table(po, "Bridge", "name", "ports", NULL)
	 || add_table(po, "Port", "interfaces", "name", "tag", "trunks", "bond_mode", NULL)
	 || add_table(po, "Interface", "name", "type", "options", "admin_state", "link_state", NULL))
		goto err_po;

	if (!(params = json_array()))
		goto err_po;

	if (json_array_append_new(params, json_string("Open_vSwitch"))
	 || json_array_append_new(params, json_null())
	 || json_array_append(params, po))
		goto err_params;

	if (!(root = json_object()))
		goto err_params;

	if (json_object_set_new(root, "method", json_string("monitor"))
	 || json_object_set_new(root, "id", json_integer(0))
	 || json_object_set(root, "params", params))
		goto err_root;

	res = json_dumps(root, 0);

err_root:
	json_decref(root);
err_params:
	json_decref(params);
err_po:
	json_decref(po);
	return res;
}

static int connect_ovs(void)
{
	int fd, err;
	struct sockaddr_un sun;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		return -errno;

	sun.sun_family = AF_UNIX;
	strncpy(sun.sun_path, db, UNIX_PATH_MAX);
	sun.sun_path[UNIX_PATH_MAX - 1] = '\0';

	if ((err = connect(fd, (struct sockaddr *)&sun,
			   sizeof(sun.sun_family) + strlen(sun.sun_path) + 1)))
		return -errno;

	return fd;
}

#define CHUNK	65536
static char *read_all(int fd)
{
	char *buf, *newbuf;
	size_t len = 0;
	ssize_t res;

	if (!(buf = malloc(CHUNK)))
		return NULL;

	while (1) {
		if ((res = read(fd, buf + len, CHUNK)) < 0)
			goto err;
		len += res;
		if (res < CHUNK) {
			buf[len] = '\0';
			return buf;
		}
		if (!(newbuf = realloc(buf, len + CHUNK)))
			goto err;
		buf = newbuf;
	}

err:
	free(buf);
	return NULL;
}

static int check_vport(struct netns_entry *ns, struct if_entry *entry)
{
	struct nl_handle hnd;
	struct ovs_header oh = { .dp_ifindex = 0 };
	struct nlmsg *req, *resp;
	int err = ENOMEM;

	/* Be paranoid. If anything goes wrong, assume the interace is not
	 * a vport. It's better to present an interface as unconnected to
	 * the bridge when it's in fact connected, than vice versa.
	 */
	if (!vport_genl_id)
		return 0;
	if (netns_switch(ns))
		return 0;
	if (genl_open(&hnd))
		return 0;

	req = genlmsg_new(vport_genl_id, OVS_VPORT_CMD_GET, 0);
	if (!req)
		goto out_hnd;
	if (nlmsg_put(req, &oh, sizeof(oh)) ||
	    nla_put_str(req, OVS_VPORT_ATTR_NAME, entry->if_name))
		goto out_req;
	err = nl_exchange(&hnd, req, &resp);
	if (err)
		goto out_req;
	/* Keep err = 0. We're only interested whether the call succeeds or
	 * not, we don't care about the returned data.
	 */
	nlmsg_free(resp);
out_req:
	nlmsg_free(req);
out_hnd:
	nl_close(&hnd);
	return !err;
}

static int link_iface_search(struct if_entry *entry, void *arg)
{
	struct ovs_if *iface = arg;
	struct ovs_if *master = list_head(iface->port->bridge->system->ifaces);
	int search_for_system = !master->link;
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

	/* We've got a match. This still may not mean the interface is
	 * actually connected in the kernel datapath. Newer kernels set
	 * master (at least for netdev interface type) to ovs-system, which
	 * we check above. For older kernels, we need to be more clever.
	 */
	if (!search_for_system && !entry->master &&
	    !check_vport(master->link->ns, entry))
		return 0;

	weight = 1;
	if (!search_for_system) {
		if (master->link->ns == entry->ns)
			weight++;
	} else {
		if (!entry->ns->name)
			weight++;
	}
	return weight;
}

static int link_iface(struct ovs_if *iface, struct list *netns_list, int required)
{
	struct netns_entry *root = list_head(*netns_list);
	struct match_desc match;
	int err;

	if (iface->link)
		return 0;

	match_init(&match);
	match.netns_list = netns_list;

	if ((err = match_if(&match, link_iface_search, iface)))
		return err;
	iface->link = match_found(match);
	if (match_ambiguous(match)) {
		label_add(&root->warnings,
				 "Failed to map openvswitch interface %s reliably",
				 iface->name);
		return ERANGE;
	}
	if (required && !iface->link) {
		label_add(&root->warnings,
				 "Failed to map openvswitch interface %s",
				 iface->name);
		return ENOENT;
	}
	return 0;
}

static struct if_entry *create_iface(char *name, char *br_name, struct netns_entry *root)
{
	struct if_entry *entry;

	entry = if_create();
	if (!entry)
		return NULL;

	asprintf(&entry->internal_ns, "ovs:%s", br_name);
	if (!entry->internal_ns)
		goto err_entry;

	entry->if_name = strdup(name);
	if (!entry->if_name)
		goto err_name;

	entry->ns = root;
	entry->flags |= IF_INTERNAL;
	list_append(&root->ifaces, node(entry));
	return entry;

err_name:
	free(entry->if_name);
err_entry:
	free(entry);
	return NULL;
}

static void label_iface(struct ovs_if *iface)
{
	if (iface->type && *iface->type)
		if_add_config(iface->link, "type", "%s", iface->type);
	if (iface->local_ip)
		if_add_config(iface->link, "from", "%s", iface->local_ip);
	if (iface->remote_ip)
		if_add_config(iface->link, "to", "%s", iface->remote_ip);
	if (iface->key)
		if_add_config(iface->link, "key", "%s", iface->key);
}

static void label_port_or_iface(struct ovs_port *port, struct if_entry *link)
{
	if (port->tag) {
		 if (asprintf(&link->edge_label, "tag %u", port->tag) < 0)
			link->edge_label = NULL;
	} else if (port->trunks_count) {
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
		if_add_config(link, "bond mode", "%s", port->bond_mode);
}

static void link_tunnel(struct ovs_if *iface)
{
	if (!iface->local_ip || !*iface->local_ip)
		return;
	link_set(tunnel_find_str(iface->link->ns, iface->local_ip), iface->link);
	iface->link->flags |= IF_LINK_WEAK;
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

static int link_patch(struct ovs_if *iface, struct list *netns_list)
{
	int err;
	struct match_desc match;

	match_init(&match);
	match.netns_list = netns_list;

	if ((err = match_if(&match, link_patch_search, iface)))
		return err;
	if (match_ambiguous(match))
		return if_add_warning(iface->link, "failed to find openvswitch patch port peer reliably");

	if (match_found(match))
		peer_set(iface->link, match_found(match));
	/* Ignore case when the peer is not found, it will be found from the
	 * other side. */
	return 0;
}

static int link_ifaces(struct list *netns_list, struct netns_entry *root)
{
	struct ovs_bridge *br;
	struct ovs_port *port;
	struct ovs_if *iface, *ovs_master;
	struct if_entry *master;
	int err;

	list_for_each(br, br_list) {
		if (!br->system || !br->system->iface_count) {
			label_add(&root->warnings,
				  "Failed to find main interface for openvswitch bridge %s",
				  br->name);
			continue;
		}
		if (br->system->iface_count > 1) {
			label_add(&root->warnings,
				  "Main port for openvswitch bridge %s appears to have several interfaces",
				  br->name);
			continue;
		}

		ovs_master = list_head(br->system->ifaces);
		if ((err = link_iface(ovs_master, netns_list, 1))) {
			if (err == ENOENT || err == ERANGE)
				continue; // Warning already reported by link_iface
			else
				return err;
		}

		list_for_each(port, br->ports) {
			master = ovs_master->link;
			if (port->iface_count > 1) {
				port->link = create_iface(port->name, port->bridge->name, root);
				if (!port->link)
					return ENOMEM;
				master_set(master, port->link);
				master = port->link;
				label_port_or_iface(port, port->link);
			}
			list_for_each(iface, port->ifaces) {
				if ((err = link_iface(iface, netns_list, 0)))
					return err;
				if (!iface->link) {
					iface->link = create_iface(iface->name,
								   iface->port->bridge->name,
								   root);
					if (!iface->link)
						return ENOMEM;
				}

				/* reconnect to the ovs master */
				master_set(master, iface->link);

				label_iface(iface);
				if (port->iface_count == 1)
					label_port_or_iface(port, iface->link);
				if (iface_is_tunnel(iface))
					link_tunnel(iface);
				else if (!strcmp(iface->type, "patch")) {
					if ((err = link_patch(iface, netns_list)))
						return err;
				}
			}
		}
	}
	return 0;
}

static int ovs_global_post(struct list *netns_list)
{
	struct netns_entry *root = list_head(*netns_list);
	char *result, *query;
	int fd, len;
	int err;

	query = construct_query();
	if (!query)
		return ENOMEM;

	len = strlen(query);
	if ((fd = connect_ovs()) < 0) {
		err = -fd;
		goto err_query;
	}

	if (write(fd, query, len) < len) {
		err = errno;
		goto err_fd;
	}

	if (!(result = read_all(fd))) {
		err = ENOMEM;
		goto err_fd;
	}

	if ((err = parse(&br_list, result, &root->warnings)))
		goto err_result;

	/* Ignore empty result */
	if (list_empty(br_list))
		goto err_result;

	if ((err = link_ifaces(netns_list, root)))
		goto err_result;

err_result:
	free(result);
err_fd:
	close(fd);
err_query:
	free(query);

	/* Errors from OVS handler are not fatal for plotnetcfg. Moreover, the
	 * interesting ones are usually reported through frontend, which will
	 * not be executed if we blow up here.
	 */
	if (err != ENOMEM)
		err = 0;

	return err;
}

static int ovs_global_init(void)
{
	struct nl_handle hnd;
	int err;

	if ((err = genl_open(&hnd))) {
		vport_genl_id = 0;
		return 0; /* intentionally ignored */
	}
	vport_genl_id = genl_family_id(&hnd, OVS_VPORT_FAMILY);
	nl_close(&hnd);
	return 0;
}

static void destruct_if(struct ovs_if *iface)
{
	free(iface->name);
	free(iface->type);
	free(iface->local_ip);
	free(iface->remote_ip);
	free(iface->key);
	free(iface->peer);
}

static void destruct_port(struct ovs_port *port)
{
	free(port->name);
	free(port->trunks);
	free(port->bond_mode);
	free(port->trunks);
	list_free(&port->ifaces, (destruct_f)destruct_if);
}

static void destruct_bridge(struct ovs_bridge *br)
{
	free(br->name);
	list_free(&br->ports, (destruct_f)destruct_port);
}

static void ovs_global_cleanup(_unused struct list *netns_list)
{
	list_free(&br_list, (destruct_f)destruct_bridge);
}

static struct global_handler gh_ovs = {
	.init = ovs_global_init,
	.post = ovs_global_post,
	.cleanup = ovs_global_cleanup,
};

static struct arg_option options[] = {
	{ .long_name = "ovs-db", .short_name = 'D', .has_arg = 1,
	  .type = ARG_CHAR, .action.char_var = &db,
	  .help = "path to openvswitch database" },
};

void handler_openvswitch_register(void)
{
	db = OVS_DB_DEFAULT;
	arg_register_batch(options, ARRAY_SIZE(options));
	global_handler_register(&gh_ovs);
}
