CC = gcc
CFLAGS = -Wall -g

CLIENT_OBJS = client.o common.o
SERVER_OBJS = server.o common.o

all: client server

client: $(CLIENT_OBJS)
server: $(SERVER_OBJS)

common.o: common.c
	$(CC) $(CFLAGS) -c common.c

client.o: client.c
	$(CC) $(CFLAGS) -c client.c

server.o: server.c
	$(CC) $(CFLAGS) -c server.c

clean:
	rm -f *.o client server
