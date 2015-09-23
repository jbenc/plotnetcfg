ifeq ($(jansson),)
libs=$(shell pkg-config --libs jansson)
else
libs=$(jansson)/src/.libs/libjansson.a
CPPFLAGS+=-I$(jansson)/src
endif
CFLAGS?=-W -Wall

all: check-libs plotnetcfg

plotnetcfg: args.o ethtool.o frontend.o handler.o if.o label.o main.o match.o netlink.o \
	    netns.o tunnel.o utils.o \
	    handlers/bridge.o handlers/master.o handlers/openvswitch.o handlers/veth.o \
	    handlers/vlan.o \
	    frontends/dot.o frontends/json.o
	gcc $(LDFLAGS) -o $@ $+ $(libs)

args.o: args.c args.h
ethtool.o: ethtool.c ethtool.h
frontend.o: frontend.c frontend.h args.h utils.h
handler.o: handler.c handler.h if.h netns.h
if.o: if.c if.h compat.h ethtool.h handler.h label.h netlink.h utils.h
label.o: label.h label.c utils.h
main.o: main.c args.h frontend.h handler.h netns.h utils.h version.h
match.o: match.c match.h if.h netns.h
netlink.o: netlink.c netlink.h utils.h
netns.o: netns.c netns.h compat.h handler.h if.h match.h utils.h
tunnel.o: tunnel.c tunnel.h handler.h if.h match.h netns.h utils.h tunnel.h
utils.o: utils.c utils.h if.h netns.h

handlers/bridge.o: handlers/bridge.c handlers/bridge.h handler.h
handlers/master.o: handlers/master.c handlers/master.h handler.h match.h utils.h
handlers/openvswitch.o: handlers/openvswitch.h args.h handler.h label.h match.h netlink.h tunnel.h utils.h
handlers/veth.o: handlers/veth.c handlers/veth.h handler.h match.h utils.h
handlers/vlan.o: handlers/vlan.c handlers/vlan.h handler.h netlink.h

frontends/dot.o: frontends/dot.c frontends/dot.h frontend.h handler.h if.h label.h netns.h \
		 utils.h version.h
frontends/json.o: frontends/json.c frontends/json.h frontend.h if.h label.h netns.h utils.h \
		  version.h

version.h:
	echo "#define VERSION \"`git describe 2> /dev/null || cat version`\"" > version.h

clean:
	rm -f version.h *.o frontends/*.o handlers/*.o plotnetcfg

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
