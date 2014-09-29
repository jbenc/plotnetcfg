#ifndef _TUNNEL_H
#define _TUNNEL_H

struct if_entry;
struct netns_entry;

struct if_entry *tunnel_find_iface(struct netns_entry *ns, const char *addr);

#endif
