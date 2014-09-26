#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <libnetlink.h>
#include "ethtool.h"
#include "handler.h"
#include "utils.h"
#include "if.h"

struct nlmsg_list
{
	struct nlmsg_list *next;
	struct nlmsghdr	  h;
};

struct nlmsg_chain
{
	struct nlmsg_list *head;
	struct nlmsg_list *tail;
};

static int store_nlmsg(_unused const struct sockaddr_nl *who,
		       struct nlmsghdr *n, void *arg)
{
	struct nlmsg_chain *lchain = (struct nlmsg_chain *)arg;
	struct nlmsg_list *h;

	h = malloc(n->nlmsg_len + sizeof(void *));
	if (h == NULL)
		return -1;

	memcpy(&h->h, n, n->nlmsg_len);
	h->next = NULL;

	if (lchain->tail)
		lchain->tail->next = h;
	else
		lchain->head = h;
	lchain->tail = h;

	return 0;
}

static void free_nlmsg_chain(struct nlmsg_chain *info)
{
	list_free(info->head, NULL);
}

static void fill_if_link(struct if_entry *dest, struct nlmsghdr *n)
{
	struct ifinfomsg *ifi = NLMSG_DATA(n);
	struct rtattr *tb[IFLA_MAX + 1];
	int len = n->nlmsg_len;

	if (n->nlmsg_type != RTM_NEWLINK)
		return;
	len -= NLMSG_LENGTH(sizeof(*ifi));
	if (len < 0)
		return;
	parse_rtattr(tb, IFLA_MAX, IFLA_RTA(ifi), len);
	if (tb[IFLA_IFNAME] == NULL)
		return;
	dest->if_index = ifi->ifi_index;
	dest->if_name = strdup(rta_getattr_str(tb[IFLA_IFNAME]));
	dest->if_flags = ifi->ifi_flags;
	if (tb[IFLA_MASTER])
		dest->master_index = *(int *)RTA_DATA(tb[IFLA_MASTER]);
	if (tb[IFLA_LINK])
		dest->link_index = *(int*)RTA_DATA(tb[IFLA_LINK]);

	dest->driver = ethtool_driver(dest->if_name);

	handler_netlink(dest, tb);
}

static char *format_addr(const struct ifaddrmsg *ifa, const void *src)
{
	char buf[64];
	int len;

	if (!inet_ntop(ifa->ifa_family, src, buf, sizeof(buf)))
		return NULL;
	len = strlen(buf);
	snprintf(buf + len, sizeof(buf) - len, "/%d", ifa->ifa_prefixlen);
	return strdup(buf);
}


static int fill_if_addr(struct if_entry *dest, struct nlmsg_list *ainfo)
{
	struct if_addr_entry *entry, *ptr = NULL;
	struct nlmsghdr *n;
	struct ifaddrmsg *ifa;
	struct rtattr *rta_tb[IFA_MAX + 1];
	int len;

	for (; ainfo; ainfo = ainfo->next) {
		n = &ainfo->h;
		ifa = NLMSG_DATA(n);
		if (ifa->ifa_index != dest->if_index)
			continue;
		if (n->nlmsg_type != RTM_NEWADDR)
			continue;
		len = n->nlmsg_len - NLMSG_LENGTH(sizeof(*ifa));
		if (len < 0)
			continue;
		if (ifa->ifa_family != AF_INET &&
		    ifa->ifa_family != AF_INET6)
			/* only IP addresses supported (at least for now) */
			continue;
		parse_rtattr(rta_tb, IFA_MAX, IFA_RTA(ifa), len);
		if (!rta_tb[IFA_LOCAL] && !rta_tb[IFA_ADDRESS])
			/* don't care about broadcast and anycast adresses */
			continue;

		entry = calloc(sizeof(struct if_addr_entry), 1);
		if (!entry)
			return ENOMEM;
		entry->family = ifa->ifa_family;

		if (!rta_tb[IFA_LOCAL]) {
			rta_tb[IFA_LOCAL] = rta_tb[IFA_ADDRESS];
			rta_tb[IFA_ADDRESS] = NULL;
		}
		entry->addr = format_addr(ifa, RTA_DATA(rta_tb[IFA_LOCAL]));
		if (!entry->addr)
			return ENOMEM;
		if (rta_tb[IFA_ADDRESS] &&
		    memcmp(RTA_DATA(rta_tb[IFA_ADDRESS]), RTA_DATA(rta_tb[IFA_LOCAL]),
			   ifa->ifa_family == AF_INET ? 4 : 16)) {
			entry->peer = format_addr(ifa, RTA_DATA(rta_tb[IFA_ADDRESS]));
			if (!entry->peer)
				return ENOMEM;
		}

		if (!ptr)
			dest->addr = entry;
		else
			ptr->next = entry;
		ptr = entry;
	}
	return 0;
}

int if_list(struct if_entry **result, struct netns_entry *ns)
{
	struct rtnl_handle rth = { .fd = -1 };
	struct nlmsg_chain linfo = { NULL, NULL };
	struct nlmsg_chain ainfo = { NULL, NULL };
	struct nlmsg_list *l;
	struct if_entry *entry, *ptr = NULL;
	int err;

	*result = NULL;

	if (rtnl_open(&rth, 0) < 0)
		return errno ? errno : -1;
	if (rtnl_wilddump_request(&rth, AF_UNSPEC, RTM_GETLINK) < 0)
		return errno ? errno : -1;
	if (rtnl_dump_filter(&rth, store_nlmsg, &linfo) < 0)
		return errno ? errno : -1;

	if (rtnl_wilddump_request(&rth, AF_UNSPEC, RTM_GETADDR) < 0)
		return errno ? errno : -1;
	if (rtnl_dump_filter(&rth, store_nlmsg, &ainfo) < 0)
		return errno ? errno : -1;

	for (l = linfo.head; l; l = l->next) {
		entry = calloc(sizeof(struct if_entry), 1);
		if (!entry)
			return ENOMEM;
		entry->ns = ns;
		fill_if_link(entry, &l->h);
		if ((err = fill_if_addr(entry, ainfo.head)))
			return err;
		if ((err = handler_scan(entry)))
			return err;
		if (!ptr)
			*result = entry;
		else
			ptr->next = entry;
		ptr = entry;
	}

	free_nlmsg_chain(&linfo);
	rtnl_close(&rth);
	return 0;
}

static void if_addr_destruct(struct if_addr_entry *entry)
{
	free(entry->addr);
	free(entry->peer);
}

static void if_list_destruct(struct if_entry *entry)
{
	handler_cleanup(entry);
	free(entry->if_name);
	free(entry->edge_label);
	list_free(entry->addr, (destruct_f)if_addr_destruct);
}

void if_list_free(struct if_entry *list)
{
	list_free(list, (destruct_f)if_list_destruct);
}
