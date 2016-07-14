CC = gcc
A = -lcrypt
all: server client

server: 
	$(CC) server.c -o bin/server $(A)
client:
	$(CC) client.c -o bin/client $(A)

