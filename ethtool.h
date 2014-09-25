#ifndef _ETHTOOL_H
#define _ETHTOOL_H

char *ethtool_driver(const char *ifname);
unsigned int ethtool_veth_pair(const char *ifname);

#endif
