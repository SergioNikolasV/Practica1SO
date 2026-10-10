CC      = gcc
CFLAGS  = -Wall -Wextra -g
LDLIBS  = -pthread

all: server client

server: server.c common.h
	$(CC) $(CFLAGS) -o $@ server.c $(LDLIBS)

client: client.c common.h
	$(CC) $(CFLAGS) -o $@ client.c $(LDLIBS)

clean:
	rm -f server client

.PHONY: all clean