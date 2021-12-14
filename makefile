# Compilatore
CC = gcc

# Flags
CFLAGS = -std=c89 -pedantic

all: master utente

# Solo master
master : master.c 
	${CC} ${CFLAGS} master.c -o master

# Solo utente
utente: utente.c
	${CC} ${CFLAGS} utente.c -o utente
