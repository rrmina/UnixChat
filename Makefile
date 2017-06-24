CC = gcc
A = -lcrypt
all: install_curses mkdir_bin server client

server: 
	$(CC) server.c -o bin/server $(A)
client:
	$(CC) client.c -o bin/client $(A)
mkdir_bin:
	if [ ! -f /bin ]; then mkdir bin; fi;
install_curses:
	sudo apt-get install libncurses5-dev
