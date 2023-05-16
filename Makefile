CC = gcc
CCFLAGS = -Wall

COMMON=common.c
OBJ=$(patsubst %.c, %.o, $(COMMON))
CLIENT=client.c
SERVER=server.c

build: $(OBJ) server client

server: $(OBJ) $(SERVER)
	$(CC) $(CCFLAGS) $(SERVER) $(OBJ) -o server

client: $(OBJ) $(CLIENT)
	$(CC) $(CCFLAGS) $(CLIENT) $(OBJ) -o client

$(OBJ): $(COMMON)
	$(CC) $(CCFLAGS) -c $(COMMON)

clean:
	@rm -f client server $(OBJ)
