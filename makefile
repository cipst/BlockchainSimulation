# Compilatore
CC = gcc

# Flags
CFLAGS = -std=c89 -pedantic

all: master utente nodo

# Solo master
master : master.c header.h
	${CC} ${CFLAGS} master.c -o master

# Solo utente
utente: utente.c header.h
	${CC} ${CFLAGS} utente.c -o utente

# Solo nodo
nodo: nodo.c header.h
	$(CC) ${CFLAGS} nodo.c -o nodo