CC      = gcc
CFLAGS  = -Wall -Wextra -g -Iinclude -O2
LDFLAGS = -lpthread

all: server client

server: server.c auction.h
	@echo "Compilation du serveur..."
	$(CC) $(CFLAGS) -o bin/server server.c $(LDFLAGS)
	@echo "✅ bin/server créé"

client: client.c auction.h
	@echo "Compilation du client..."
	$(CC) $(CFLAGS) -o bin/client client.c $(LDFLAGS)
	@echo "✅ bin/client créé"

clean:
	rm -f bin/server bin/client
	@echo "✅ Nettoyé"

run-server: server
	./bin/server

run-client: client
	./bin/client

run-client-remote: client
	./bin/client 192.168.1.100

.PHONY: all clean run-server run-client run-client-remoteS
