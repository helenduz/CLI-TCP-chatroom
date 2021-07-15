CC = gcc
CFLAGS = -Wall -g3 -fsanitize=address -pthread
PROGS =	client server

all:	$(PROGS)

%:	%.c
	$(CC) $(CFLAGS) $@.c -o $@

clean:
	rm -f $(PROGS) $(TEMPFILES) *.o