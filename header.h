#define _HEADER_H
#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>  /*  per l'I/O */
#include <stdlib.h> /*  per la exit, malloc, free, atoi, atof, etc. */
#include <string.h>
#include <sys/ipc.h>    /*  per le funzioni ipc */
#include <sys/msg.h>    /*  per le code di messaggi. */
#include <sys/sem.h>    /*  per i semafori. */
#include <sys/shm.h>    /*  per la memoria condivisa. */
#include <sys/signal.h> /*  per gestire i segnali. signal(), kill(). */
#include <sys/types.h>  /*  per la portabilità */
#include <sys/wait.h>   /*  per la wait() e la sleep(). */
#include <time.h>
#include <unistd.h> /*  per la fork, getpid, getppid, etc. */

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

/* Enumerazione per la gestione dei segnali */
enum Sem { userSync,
           nodeSync,
           userShm,
           nodeShm,
           ledgerShm,
           print };

/**
 *  »»»»»»»»»» COSTANTI di Configurazione »»»»»»»»»» 
 **/
#define SO_BLOCK_SIZE 2     /* numero di transazioni massime presenti in un blocco del libro mastro */
#define SO_REGISTRY_SIZE 10 /* numero di blocchi massimi presenti nel libro mastro */

/**
 *  »»»»»»»»»» VARIABILI di Configurazione »»»»»»»»»»
 **/
int SO_USERS_NUM;            /* numero di processi utente che possono inviare denaro ad altri utenti attraverso una transazione */
int SO_NODES_NUM;            /* numero di processi nodo che elaborano, a pagamento, le transazioni ricevute */
int SO_REWARD;               /* la percentuale di reward pagata da ogni utente per il processamento di una transazione */
long SO_MIN_TRANS_GEN_NSEC;  /* minimo valore del tempo (espresso in nanosecondi) che trascorre fra la generazione di una transazione e la seguente da parte di un utente */
long SO_MAX_TRANS_GEN_NSEC;  /* massimo valore del tempo (espresso in nanosecondi) che trascorre fra la generazione di una transazione e la seguente da parte di un utente */
int SO_RETRY;                /* numero massimo di fallimenti consecutivi nella generazione di transazioni dopo cui un processo utente termina */
int SO_TP_SIZE;              /* numero massimo di transazioni nella transaction pool dei processi nodo */
long SO_MIN_TRANS_PROC_NSEC; /* minimo valore del tempo simulato (espresso in nanosecondi) di processamento di un blocco da parte di un nodo */
long SO_MAX_TRANS_PROC_NSEC; /* massimo valore del tempo simulato (espresso in nanosecondi) di processamento di un blocco da parte di un nodo */
int SO_BUDGET_INIT;          /* budget iniziale di ciascun processo utente */
time_t SO_SIM_SEC;           /* durata della simulazione (in secondi) */
int SO_FRIENDS_NUM;          /* IMPORTANTE numero di nodi amici dei processi nodo (solo per la versione full) */
int SO_HOPS;                 /* IMPORTANTE numero massimo di salti massimi che una transazione può effettuare quando la transaction pool di un nodo è piena (solo per la versione full) */

/**
 * »»»»»»»»»» VARIABILI »»»»»»»»»»
 **/
int offset; /* offset per le memorie condivise 'users'/'nodes', ogni utente/nodo è a conoscenza della sua posizione in 'users'/'nodes' */

int semId;            /* ftok(..., 's') => 's': semaphore */
int shmLedgerId;      /* ftok(..., 'l') => 'l': ledger */
int shmUsersId;       /* ftok(..., 'u') => 'u': users */
int shmNodesId;       /* ftok(..., 'n') => 'n': nodes */
int shmActiveUsersId; /* ftok(..., 'a') => 'a': active user process */
int shmActiveNodesId; /* ftok(..., 'g') => 'g': active user process */

int* balance; /* RIFERIMENTO contenente il bilancio di ogni processo (RIFERIMENTO a (users+offset)->balance oppure (nodes+offset)->balance) */

struct sigaction act;
struct timespec tp;
struct timespec remaining, request;

typedef struct {
    unsigned long timestamp; /* timestamp della transazione con risoluzione dei nanosecondi */
    pid_t sender;            /* (implicito, in quanto è l’utente che ha generato la transazione) */
    pid_t receiver;          /* utente destinatario della somma */
    unsigned int quantity;   /* quantità di denaro inviata */
    unsigned int reward;     /* denaro pagato dal sender al nodo che processa la transazione */
} transaction;

typedef struct {
    int size;
    transaction transaction[SO_BLOCK_SIZE];
} block;

/* »»»»»»»»»» Libro Mastro »»»»»»»»»» */
typedef struct {
    int size;
    block block[SO_REGISTRY_SIZE];
} ledger;

/* »»»»»»»»»» Processo Utente »»»»»»»»»» */
typedef struct {
    pid_t pid;
    int balance;
} userProcess;

/* »»»»»»»»»» Processo Nodo »»»»»»»»»» */
typedef struct {
    pid_t pid;
    int balance;
    int poolSize;
} nodeProcess;

typedef struct {
    long mtype;
    transaction transaction;
} message;

ledger* mastro;          /* informazioni sul libro mastro in memoria condivisa */
userProcess* users;      /* informazioni sugli utenti in memoria condivisa */
nodeProcess* nodes;      /* informazione sui nodi in memoria condivisa */
int* activeUsers;        /* conteggio dei processi utente attivi in shared memory */
int* activeNodes;        /* conteggio dei processi nodo attivi in shared memory */
transaction lastVisited; /* ultima transazione visitata per il conteggio del bilancio */

/**
 *  »»»»»»»»»» GESTIONE SEMAFORI »»»»»»»»»» 
 **/

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

/**
 * »»»»»»»»»» METODI »»»»»»»»»» 
 **/

/** Metodo che gestisce la stampa degli errori
* 	
*   @param txt Testo da stampare
**/
void error(char* txt) {
    printf("[ %sERROR%s ] %d - %s%s%s%s\n", RED, RESET, getpid(), RED, BOLD, txt, RESET);
    exit(EXIT_FAILURE);
}

/** Stampa a video le informazioni di una transazione
 * 
 * @param t Transazione
 * */
void printTransaction(transaction* t) {
    printf("\t\t%s#----Transaction----%s\n", YELLOW, RESET);
    printf("\t\t  %sTimestamp%s: %lu\n", BLUE, RESET, t->timestamp);
    printf("\t\t  %sSender%s: %d\n", BLUE, RESET, t->sender);
    printf("\t\t  %sReceiver%s: %d\n", BLUE, RESET, t->receiver);
    printf("\t\t  %sQuantity%s: %d\n", BLUE, RESET, t->quantity);
    printf("\t\t  %sReward%s: %u\n", BLUE, RESET, t->reward);
}

/** Stampa a video le informazioni di un blocco
 * 
 * @param b Blocco
 * */
void printBlock(block* b) {
    int j;
    printf("\t%s#------Block------%s\n", CYAN, RESET);
    printf("\t  %sSize%s: %d\n", GREEN, RESET, b->size);
    for (j = 0; j < b->size; ++j) {
        printTransaction(&(b->transaction[j]));
    }
}

/** Stampa a video le informazioni del Libro Mastro
 * 
 * @param l Libro Mastro
 * */
void printLedger(ledger* l) {
    int i;
    printf("\n%s»»»»»»»» Libro Mastro »»»»»»»»%s\n", MAGENTA, RESET);
    printf("  %sSize%s: %d\n", GREEN, RESET, l->size);
    for (i = 0; i < l->size; ++i) {
        printBlock(&(l->block[i]));
    }
}

/** Controlla se una transazione è già stata calcolata nel bilancio in base al suo timestamp
 * 
 * @param lastVisited puntatore all'ultima transazione visitata
 * @param trans puntatore alla transazione da verificare
 * 
 * @return 0 se è già stata calcolata nel bilancio, -1 altrimenti
  **/
int alreadyVisited(transaction* lastVisited, transaction* trans) {
    int already = -1;

    if (trans->timestamp <= lastVisited->timestamp) { /* se TRUE vuol dire che ho già visitato questa transazione */
        already = 0;
    } else {
        (*lastVisited) = (*trans);
    }
    return already;
}

/** Calcola il bilancio delle transazioni presenti nel Libro Mastro 
 * 
 * @param mastro puntatore al libro mastro
 * @param identifier pid/ID unico per riconoscimento di utente/nodo
 * @param lastVisited puntatore all'ultima transazione visitata (già contata dal libro mastro)
 * 
 * @return il bilancio (entrate) delle transazioni che l'utente ha ricevuto
 */
int balanceFromLedger(ledger* mastro, pid_t identifier, transaction* lastVisited) {
    int i, j;
    int ris = 0;
    for (i = 0; i < mastro->size; ++i) {
        for (j = 0; j < mastro->block[i].size; ++j) {
            if (mastro->block[i].transaction[j].receiver == identifier) {
                if (alreadyVisited(lastVisited, &(mastro->block[i].transaction[j])) == -1) {
                    ris += mastro->block[i].transaction[j].quantity;
                }
            }
        }
    }
    return ris;
}

/** Inizializza le variabili neccessarie per l'esecuzione di un Utente o di un Nodo
 *  
 * @param argv vettore di stringhe contenente i parametri da inizializzare
 **/
void initVariable(char** argv) {
    SO_USERS_NUM = atoi(argv[1]);
    SO_NODES_NUM = atoi(argv[2]);
    SO_BUDGET_INIT = atoi(argv[3]);
    SO_MIN_TRANS_GEN_NSEC = atol(argv[5]);
    SO_MAX_TRANS_GEN_NSEC = atol(argv[6]);
    SO_RETRY = atoi(argv[7]);
    offset = atoi(argv[8]);

    if (strcasecmp(argv[0], "./utente")) { /* controllo che sia un utente */
        SO_REWARD = atoi(argv[4]);
    } else { /* altrimenti è un nodo */
        SO_TP_SIZE = atoi(argv[4]);
    }
}