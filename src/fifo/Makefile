CC      = gcc
CFLAGS  = -Wall -Wextra -g

all: server client

server: server.c comunication.h
	$(CC) $(CFLAGS) -o $@ server.c

client: client.c comunication.h
	$(CC) $(CFLAGS) -o $@ client.c

clean:
	rm -f server client

.PHONY: all clean
