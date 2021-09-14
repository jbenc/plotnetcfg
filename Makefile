ifeq ($(jansson),)
libs=$(shell pkg-config --libs jansson)
else
libs=$(wildcard $(jansson)/src/.libs/libjansson.a $(jansson)/lib/libjansson.a)
INCLUDE=-I$(jansson)/include
endif

CFLAGS ?= -W -Wall
EXTRA_CFLAGS = -std=c99 -D_GNU_SOURCE $(INCLUDE)

OBJECTS=addr args ethtool frontend handler if label main master \
        match netlink netns route sysfs tunnel utils
HANDLERS=bond bridge geneve gre iov ipxipy macsec openvswitch team veth vlan vti vxlan xfrm route
FRONTENDS=dot json

OBJ=$(OBJECTS:%=%.o) $(HANDLERS:%=handlers/%.o) $(FRONTENDS:%=frontends/%.o)

all: check-libs plotnetcfg

plotnetcfg: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $+ $(libs)

Makefile.dep: version.h $(OBJ:.o=.c)
	$(CC) -M $(CFLAGS) $(EXTRA_CFLAGS) $(OBJ:.o=.c) | sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' >$@

-include Makefile.dep

version.h:
	echo "#define VERSION \"`git describe 2> /dev/null || cat version`\"" > version.h

%.o: %.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<

clean:
	rm -f version.h Makefile.dep *.o frontends/*.o handlers/*.o plotnetcfg

install: plotnetcfg
	install -d $(DESTDIR)/usr/sbin/
	install plotnetcfg $(DESTDIR)/usr/sbin/
	install -d $(DESTDIR)/usr/share/man/man8/
	install -d $(DESTDIR)/usr/share/man/man5/
	install -m 644 plotnetcfg.8 $(DESTDIR)/usr/share/man/man8/
	install -m 644 plotnetcfg-json.5 $(DESTDIR)/usr/share/man/man5/

.PHONY: check-libs
check-libs:
	@if [ "$(libs)" = "" ]; then \
	echo "ERROR: libjansson not found."; \
	exit 1; \
	fi
