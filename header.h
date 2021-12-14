#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>      /*  per l'I/O */
#include <stdlib.h>     /*  per la exit, malloc, free, atoi, atof, etc. */
#include <sys/ipc.h>    /*  per le funzioni ipc */
#include <sys/msg.h>    /*  per le code di messaggi. */
#include <sys/sem.h>    /*  per i semafori. */
#include <sys/shm.h>    /*  per la memoria condivisa. */
#include <sys/signal.h> /*  per gestire i segnali. signal(), kill(). */
#include <sys/types.h>  /*  per la portabilità */
#include <sys/wait.h>   /*  per la wait() e la sleep(). */
#include <unistd.h>     /*  per la fork, getpid, getppid, etc. */

#define BLACK "\033[30m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define WHITE "\033[37m"
#define UNDELINE "\033[4m"
#define BOLD "\033[1m"
#define RESET "\033[0m"

/* Transazione */
typedef struct {
    unsigned long timestamp; /* timestamp della transazione con risoluzione dei nanosecondi */
    pid_t sender;            /* (implicito, in quanto è l’utente che ha generato la transazione) */
    pid_t receiver;          /* utente destinatario della somma */
    unsigned int quantity;   /* quantità di denaro inviata */
    unsigned int reward;     /* denaro pagato dal sender al nodo che processa la transazione */
} transaction;

/* Metodo che gestisce la stampa degli errori */
void error(char* txt) {
    /**
	 * Funzione che mi aiuta nella scrittura in output degli errori.
	 * 		Parametri:
	 *			-	txt: la stringa che devo stampare
	 **/

    char aux[] = RED "Error" WHITE;
    printf("[ %s ] %d - %s\n", aux, getpid(), txt);
    exit(EXIT_FAILURE);
}