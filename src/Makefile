CC=gcc
CFLAGS=-Wall -Wextra -g
#CFLAGS=-g

rastertoql570: ql570.h ql570.c rastertoql570.h rastertoql570.c
	rm -f ../rastertoql570
	$(CC) $(CFLAGS) -lcups -lcupsimage ql570.c rastertoql570.c -o ../rastertoql570

minimal: ql570.h ql570.c examples/minimal.c
	rm -f ../minimal
	$(CC) $(CFLAGS) -lcups -lcupsimage ql570.c examples/minimal.c -o ../minimal
