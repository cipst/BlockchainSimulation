# Compilatore
CC = gcc

# Flags
CFLAGS = -std=c89 -pedantic

# Headers file
H = ./headers/header.h
MASTER_H = ./headers/master.h
USER_H = ./headers/utente.h
NODE_H = ./headers/nodo.h

# Functions file
FUNC = ./functions/functions.c
MASTER_FUNC = ./functions/master_functions.c
USER_FUNC = ./functions/utente_functions.c
NODE_FUNC = ./functions/nodo_functions.c

# GCC tutti
all: master utente nodo

# GCC solo master
master : master.c ${H} ${MASTER_H} ${FUNC} ${MASTER_FUNC}
	${CC} ${CFLAGS} ${FUNC} ${MASTER_FUNC} master.c -o master

# GCC solo utente
utente: utente.c ${H} ${USER_H} ${FUNC} ${USER_FUNC}
	${CC} ${CFLAGS} ${FUNC} ${USER_FUNC} utente.c -o utente

# GCC solo nodo
nodo: nodo.c ${H} ${NODE_H} ${FUNC} ${NODE_FUNC}
	$(CC) ${CFLAGS} ${FUNC} ${NODE_FUNC} nodo.c -o nodo