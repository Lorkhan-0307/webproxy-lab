CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lpthread

# Default target: Build all
all: proxy echo_client echo_server

# Targets for building each component separately
build_proxy: proxy
build_echo: echo_client echo_server
build_echo_client: echo_client
build_echo_server: echo_server

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

echo_server.o: echo_server.c csapp.h
	$(CC) $(CFLAGS) -c echo_server.c

echo_server: echo_server.o csapp.o
	$(CC) $(CFLAGS) echo_server.o csapp.o -o echo_server $(LDFLAGS)

echo_client.o: echo_client.c csapp.h
	$(CC) $(CFLAGS) -c echo_client.c

echo_client: echo_client.o csapp.o
	$(CC) $(CFLAGS) echo_client.o csapp.o -o echo_client $(LDFLAGS)

proxy.o: proxy.c csapp.h
	$(CC) $(CFLAGS) -c proxy.c

proxy: proxy.o csapp.o
	$(CC) $(CFLAGS) proxy.o csapp.o -o proxy $(LDFLAGS)

# Creates a tarball in ../proxylab-handin.tar that you can then hand in. DO NOT MODIFY THIS!
handin:
	(make clean; cd ..; tar cvf $(USER)-proxylab-handin.tar proxylab-handout --exclude tiny --exclude nop-server.py --exclude proxy --exclude driver.sh --exclude port-for-user.pl --exclude free-port.sh --exclude ".*")

clean:
	rm -f *~ *.o proxy echo_client echo_server core *.tar *.zip *.gzip *.bzip *.gz
