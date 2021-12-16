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

extern char** __environ;

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

#define specialSender (-1)

/* »»»»»»»»»» COSTANTI di Configurazione »»»»»»»»»» */
#define SO_BLOCK_SIZE 5     /* numero di transazioni massime presenti in un blocco del libro mastro */
#define SO_REGISTRY_SIZE 10 /* numero di blocchi massimi presenti nel libro mastro */

/* »»»»»»»»»» Transazione »»»»»»»»»» */
typedef struct {
    unsigned long timestamp; /* timestamp della transazione con risoluzione dei nanosecondi */
    pid_t sender;            /* (implicito, in quanto è l’utente che ha generato la transazione) */
    pid_t receiver;          /* utente destinatario della somma */
    unsigned int quantity;   /* quantità di denaro inviata */
    unsigned int reward;     /* denaro pagato dal sender al nodo che processa la transazione */
} transaction;

/* »»»»»»»»»» Blocco del libro mastro »»»»»»»»»» */
typedef struct {
    int size;
    transaction transaction[SO_BLOCK_SIZE];
} block;

/* »»»»»»»»»» Libro Mastro »»»»»»»»»» */
typedef struct {
    int size;
    block block[SO_REGISTRY_SIZE];
} ledger;

/* Metodo che gestisce la stampa degli errori */
void error(char* txt) {
    /**
	 * Funzione che mi aiuta nella scrittura in output degli errori.
	 * 		Parametri:
	 *			-	txt: la stringa che devo stampare
	 **/

    char aux[] = RED "Error" WHITE;
    printf("[ %s ] %d - %s\n", aux, getpid(), txt);
    perror("");
    exit(EXIT_FAILURE);
}

/* »»»»»»»»»» GESTIONE SEMAFORI »»»»»»»»»» */
union semun {
    int val;               /* value for SETVAL */
    struct semid_ds* buf;  /* buffer for IPC_STAT, IPC_SET */
    unsigned short* array; /* array for GETALL, SETALL */
/* Linux specific part */
#if defined(__linux__)
    struct seminfo* __buf; /* buffer for IPC_INFO */
#endif
};

/* Inizializza il semaforo a 1 (DISPONIBILE) */
int initSemAvailable(int semId, int semNum) {
    union semun arg;
    arg.val = 1;
    return semctl(semId, semNum, SETVAL, arg);
}

/* Inizializza il semaforo a 0 (IN USO) */
int initSemInUse(int semId, int semNum) {
    union semun arg;
    arg.val = 0;
    return semctl(semId, semNum, SETVAL, arg);
}

/* Riserva il semaforo - decrementa di 1 */
int reserveSem(int semId, int semNum) {
    struct sembuf sops;
    sops.sem_num = semNum;
    sops.sem_op = -1;
    sops.sem_flg = 0;
    return semop(semId, &sops, 1);
}

/* Rilascia il semaforo - incrementa di 1 */
int releaseSem(int semId, int semNum) {
    struct sembuf sops;
    sops.sem_num = semNum;
    sops.sem_op = 1;
    sops.sem_flg = 0;
    return semop(semId, &sops, 1);
}

/* Rimuove il semaforo (IPC_RMID) */
int removeSem(int semId, int semNum) {
    union semun arg;
    arg.val = 0;
    return semctl(semId, semNum, IPC_RMID, arg);
}

/* »»»»»»»»»» Stampa Libro Mastro »»»»»»»»»» */
void printLedger(ledger* l) {
    int i, j;
    printf("\n%s»»»»»»»» Libro Mastro »»»»»»»»%s\n", MAGENTA, WHITE);
    printf("  %sSize%s: %d\n", GREEN, WHITE, l->size);
    for (i = 0; i < l->size; ++i) {
        printf("\t%s#------Block %d------%s\n", CYAN, i, WHITE);
        printf("\t  %sSize%s: %d\n", GREEN, WHITE, l->block[i].size);
        for (j = 0; j < l->block[i].size; ++j) {
            printf("\t\t%s#----Transaction %d----%s\n", YELLOW, j, WHITE);
            printf("\t\t  %sTimestamp%s: %lu\n", BLUE, WHITE, l->block[i].transaction[j].timestamp);
            printf("\t\t  %sSender%s: %d\n", BLUE, WHITE, l->block[i].transaction[j].sender);
            printf("\t\t  %sReceiver%s: %d\n", BLUE, WHITE, l->block[i].transaction[j].receiver);
            printf("\t\t  %sQuantity%s: %d\n", BLUE, WHITE, l->block[i].transaction[j].quantity);
            printf("\t\t  %sReward%s: %u\n", BLUE, WHITE, l->block[i].transaction[j].reward);
        }
    }
}