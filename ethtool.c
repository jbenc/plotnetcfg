#include <errno.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "ethtool.h"

#include <stdio.h>

char *ethtool_driver(const char *ifname)
{
	int fd;
	struct ifreq ifr;
	struct ethtool_drvinfo info;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		return NULL;
	memset(&ifr, 0, sizeof(ifr));
	memset(&info, 0, sizeof(info));
	strcpy(ifr.ifr_name, ifname);
	ifr.ifr_data = (caddr_t)&info;
	info.cmd = ETHTOOL_GDRVINFO;
	if (ioctl(fd, SIOCETHTOOL, &ifr) < 0) {
		close(fd);
		return NULL;
	}
	close(fd);
	return strdup(info.driver);
}
