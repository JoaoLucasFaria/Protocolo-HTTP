CC=gcc
CFLAGS=-O2 -Wall -Wextra

all: servidor

servidor: src/servidor.c src/mime.c src/http.h
	$(CC) $(CFLAGS) -o meu_servidor src/servidor.c src/mime.c

clean:
	rm -f meu_servidor
