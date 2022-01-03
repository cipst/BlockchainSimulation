# Compilatore
CC = gcc

# Flags
CFLAGS = -std=c89 -pedantic

FUNC = ./functions/functions.c
MASTER_FUNC = ./functions/master_functions.c
USER_FUNC = ./functions/utente_functions.c
NODE_FUNC = ./functions/nodo_functions.c

all: master utente nodo

# Solo master
master : master.c header.h ${FUNC} ${MASTER_FUNC}
	${CC} ${CFLAGS} ${FUNC} ${MASTER_FUNC} master.c -o master

# Solo utente
utente: utente.c header.h ${FUNC} ${USER_FUNC}
	${CC} ${CFLAGS} ${FUNC} ${USER_FUNC} utente.c -o utente

# Solo nodo
nodo: nodo.c header.h ${FUNC} ${NODE_FUNC}
	$(CC) ${CFLAGS} ${FUNC} ${NODE_FUNC} nodo.c -o nodo