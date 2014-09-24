CFLAGS=-W -Wall

plotnetcfg: if.o main.o netns.o utils.o
	gcc -o $@ $+ /usr/lib64/libnetlink.a

if.o: if.c if.h utils.h
main.o: main.c if.h netns.h utils.h
netns.o: netns.c netns.h if.h utils.h
utils.o: utils.c utils.h
