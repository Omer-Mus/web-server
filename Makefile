CC = gcc
CFLAGS   = -g -Wall
LDFLAGS = -g

http-server: http-server.c

.PHONY: clean
clean:
	rm -f *.o a.out core http-server
