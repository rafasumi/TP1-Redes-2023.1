CC = gcc
CCFLAGS = -Wall

build:
	$(CC) $(CCFLAGS) -c common.c
	$(CC) $(CCFLAGS) client.c common.o -o client
	$(CC) $(CCFLAGS) server.c common.o -o server

clean:
	@rm -f client server *.o
