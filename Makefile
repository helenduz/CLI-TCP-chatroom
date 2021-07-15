CC = gcc
CFLAGS = -Wall -g3 -fsanitize=address -pthread
PROGS =	client server

all:	$(PROGS)

%:	%.c
	$(CC) $(CFLAGS) $@.c -o $@

client: client.c
	$(CC) $(CFLAGS) client.c -o client

server: server.c
	$(CC) $(CFLAGS) server.c -o server

clean:
	rm -f $(PROGS) $(TEMPFILES) *.o
