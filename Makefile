CC=gcc
CFLAGS=-O2 -Wall -Wextra

all: servidor cliente

servidor: src/servidor.c src/mime.c src/http.h
	$(CC) $(CFLAGS) -o meu_servidor src/servidor.c src/mime.c

cliente: src/cliente.c
	$(CC) $(CFLAGS) -o meu_navegador src/cliente.c

clean:
	rm -f meu_servidor meu_navegador
