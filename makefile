# Compilatore
CC = gcc

# Flags
CFLAGS = -std=c89 -pedantic

all: master utente nodo

# Solo master
master : master.c 
	${CC} ${CFLAGS} master.c -o master

# Solo utente
utente: utente.c
	${CC} ${CFLAGS} utente.c -o utente

# Solo nodo
nodo: nodo.c
	$(CC) ${CFLAGS} nodo.c -o nodo
