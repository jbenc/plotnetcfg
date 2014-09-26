CFLAGS=-W -Wall

plotnetcfg: dot.o ethtool.o handler.o if.o main.o netns.o utils.o \
	    parson/parson.o \
	    handlers/master.o handlers/openvswitch.o handlers/veth.o
	gcc -o $@ $+ /usr/lib64/libnetlink.a

dot.o: dot.c dot.h handler.h if.h netns.h utils.h
ethtool.o: ethtool.c ethtool.h
handler.o: handler.c handler.h if.h netns.h
if.o: if.c if.h ethtool.h handler.h utils.h
main.o: main.c dot.h netns.h utils.h
netns.o: netns.c netns.h handler.h if.h utils.h
utils.o: utils.c utils.h if.h netns.h

parson/parson.c: parson/parson.h

handlers/master.o: handlers/master.c handlers/master.h handler.h utils.h
handlers/openvswitch.o: handlers/openvswitch.h parson/parson.h handler.h utils.h
handlers/veth.o: handlers/veth.c handlers/veth.h handler.h utils.h

clean:
	rm -f *.o handlers/*.o plotnetcfg
