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

#define _GNU_SOURCE
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <linux/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <jansson.h>
#include "../handler.h"
#include "../label.h"
#include "../if.h"
#include "../utils.h"
#include "team.h"

#define TEAMD_REQUEST_PREFIX	"REQUEST"
#define TEAMD_ERR_PREFIX	"REPLY_ERROR"
#define TEAMD_SUCC_PREFIX	"REPLY_SUCCESS"
#define TEAMD_SOCK_PATH		"/var/run/teamd/"

#define TEAMD_REPLY_TIMEOUT 5000
#define TEAMD_REQ TEAMD_REQUEST_PREFIX "\nStateDump\n"

static int team_scan(struct if_entry *entry);
static int team_post(struct if_entry *entry, struct netns_entry *root);
static void team_cleanup(struct if_entry *entry);

static struct handler h_team = {
	.driver = "team",
	.scan = team_scan,
	.post = team_post,
	.cleanup = team_cleanup
};

struct team_priv {
	json_t *active_port_name;
};

void handler_team_register(void)
{
	handler_register(&h_team);
}

static int team_connect(struct if_entry *entry)
{
	int fd, fl;
	struct sockaddr_un addr;

	fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (fd < 0)
		return -errno;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, UNIX_PATH_MAX, TEAMD_SOCK_PATH"%s.sock", entry->if_name);

	fl = fcntl(fd, F_GETFL);
	fl |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, fl) == -1)
		goto err;

	if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)))
		goto err;

	return fd;
err:
	close(fd);
	return -errno;
}

static int team_wait(int fd)
{
	fd_set rfds;
	struct timeval tv;
	int ret;

	tv.tv_sec = TEAMD_REPLY_TIMEOUT / 1000;
	tv.tv_usec = TEAMD_REPLY_TIMEOUT % 1000 * 1000;
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);

	ret = select(fd + 1, &rfds, NULL, NULL, &tv);
	if (ret < 0)
		return errno;

	if (!FD_ISSET(fd, &rfds))
		return ETIMEDOUT;

	return 0;
}

static char *team_recv(int fd)
{
	int size, len, r;
	char *buf, *newbuf;

	size = 4096;
	len = 0;
	buf = malloc(size);
	if (!buf)
		return NULL;

	while ((r = read(fd, buf + len, size - len))) {
		if (r < 0) {
			free(buf);
			return NULL;
		}

		len += r;
		if (len < size)
			break;

		size *= 2;
		newbuf = realloc(buf, size);
		if (!newbuf) {
			free(buf);
			return NULL;
		}
		buf = newbuf;
	}

	buf[len] = '\0';
	return buf;
}

static char *team_getline(char **reply)
{
	char *nl, *ret;

	nl = strchr(*reply, '\n');
	if (nl == NULL) {
		ret = *reply;
		*reply = "";
		return ret;
	}

	*nl = '\0';
	ret = *reply;
	*reply = nl + 1;
	return ret;
}

int team_check_if(json_t *jdev, struct if_entry *entry, json_error_t *jerr)
{
	char *ifname;
	unsigned int ifindex;

	if (json_unpack_ex(jdev, jerr, 0, "{s:{s:i,s:s}}", "ifinfo", "ifindex", &ifindex, "ifname", &ifname))
		return -1;

	return ifindex != entry->if_index || strcmp(ifname, entry->if_name);
}

int team_parse_runner(json_t *jrunner, struct if_entry *entry, _unused json_error_t *jerr)
{
	struct team_priv *priv = entry->handler_private;
	json_t *jport;

	if (jrunner == NULL)
		return 0;

	jport = json_object_get(jrunner, "active_port");
	if (jport) {
		priv->active_port_name = jport;
		json_incref(jport);
	}

	return 0;
}

int team_parse_setup(json_t *jsetup, struct if_entry *entry, json_error_t *jerr)
{
	char *runner_name;

	if (json_unpack_ex(jsetup, jerr, 0, "{s:s}", "runner_name", &runner_name))
		return -1;

	label_add(&entry->label, "runner: %s", runner_name);
	return 0;
}


static void team_parse_json(char *json, struct if_entry *entry)
{
	json_error_t jerr;
	json_t *jroot, *jports, *jsetup, *jdev, *jrunner;
	char *errstr;

	errstr = NULL;

	jroot = json_loads(json, 0, &jerr);
	if (!jroot) {
		errstr = "Failed to parse JSON.";
		goto error_load;
	}

	jrunner = jports = NULL;
	if (json_unpack_ex(jroot, &jerr, 0, "{s:o,s:o,s?:o,s?:o}", "setup", &jsetup, "team_device", &jdev, "runner", &jrunner)) {
		errstr = jerr.text;
		goto error_jroot;
	}

	if (team_check_if(jdev, entry, &jerr)) {
		errstr = "Daemon controling different device";
		goto error_jroot;
	}

	if (team_parse_setup(jsetup, entry, &jerr)) {
		errstr = jerr.text;
		goto error_jroot;
	}

	if (team_parse_runner(jrunner, entry, &jerr)) {
		errstr = jerr.text;
		goto error_jroot;
	}

error_jroot:
	json_decref(jroot);
error_load:
	if (errstr)
		if_add_warning(entry, "Team: Error parsing reply (%s)", errstr);

	return;
}

static void team_parse_reply(char *reply, struct if_entry *entry)
{
	char *rest, *line;

	rest = reply;
	line = team_getline(&rest);
	if (strncmp(line, TEAMD_SUCC_PREFIX, sizeof(TEAMD_SUCC_PREFIX)) == 0) {
		team_parse_json(rest, entry);
		return;
	}
	if (strncmp(line, TEAMD_ERR_PREFIX, sizeof(TEAMD_ERR_PREFIX)) == 0) {
		line = team_getline(&rest);
		if_add_warning(entry, "Team: Received error reply (%s: %s)", line, rest);
		return;
	}
	if_add_warning(entry, "Team: Cannot parse reply");
}

static int team_scan(struct if_entry *entry)
{
	int fd, err;
	char *reply;

	entry->handler_private = calloc(1, sizeof(struct team_priv));
	if (!entry->handler_private)
		return ENOMEM;

	fd = team_connect(entry);
	if (fd < 0) {
		if_add_warning(entry, "Team: Failed to connect to teamd (%s)", strerror(-fd));
		return 0;
	}

	if (write(fd, TEAMD_REQ, sizeof(TEAMD_REQ)) < (ssize_t) sizeof(TEAMD_REQ)) {
		if_add_warning(entry, "Team: Failed to send request (%s)", strerror(errno));
		goto error_conn;
	}

	if ((err = team_wait(fd))) {
		if_add_warning(entry, "Team: Failed to get status (%s)", strerror(err));
		goto error_conn;
	}

	reply = team_recv(fd);
	if (!reply) {
		err = errno;
		if_add_warning(entry, "Team: Failed to receive reply (%s)", strerror(errno));
		goto error_conn;
	}

	team_parse_reply(reply, entry);
	free(reply);

error_conn:
	close(fd);
	return 0;
}

static int team_post(struct if_entry *master, _unused struct netns_entry *root)
{
	struct team_priv *priv = master->handler_private;
	struct if_list_entry *le;
	const char *active_name;

	if (!master->active_slave && priv->active_port_name) {
		active_name = json_string_value(priv->active_port_name);
		for (le = master->rev_master; le; le = le->next) {
			if (strcmp(le->entry->if_name, active_name)) {
				le->entry->flags |= IF_PASSIVE_SLAVE;
			} else {
				master->active_slave = le->entry;
				label_add(&master->label, "active port: %s", le->entry->if_name);
			}
		}
	}
	return 0;
}

static void team_cleanup(struct if_entry *entry)
{
	struct team_priv *priv = entry->handler_private;

	if (priv->active_port_name)
		json_decref(priv->active_port_name);

	free(entry->handler_private);
}
