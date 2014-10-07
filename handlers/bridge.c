#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include "../handler.h"
#include "bridge.h"

static int bridge_scan(struct if_entry *entry);

static struct handler h_bridge = {
	.scan = bridge_scan,
};

void handler_bridge_register(void)
{
	handler_register(&h_bridge);
}

static int bridge_scan(struct if_entry *entry)
{
	int fd, res;
	char buf[37 + IFNAMSIZ + 1];

	if (entry->master_index)
		return 0;
	snprintf(buf, sizeof(buf), "/sys/class/net/%s/brport/bridge/ifindex", entry->if_name);
	fd = open(buf, O_RDONLY);
	if (fd < 0) {
		if (errno == ENOENT)
			return 0;
		return errno;
	}
	res = read(fd, buf, sizeof(buf));
	if (res < 0) {
		res = errno;
		goto out;
	}
	if (res == 0) {
		res = EINVAL;
		goto out;
	}
	entry->master_index = atoi(buf);
	res = 0;
out:
	close(fd);
	return res;
}
