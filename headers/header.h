#ifndef _HEADER_H
#define _HEADER_H
#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>  /*  per l'I/O */
#include <stdlib.h> /*  per la exit, malloc, free, atoi, atol, etc. */
#include <string.h>
#include <sys/ipc.h>    /*  per le funzioni ipc */
#include <sys/mman.h>   /* utilizzato per ridimensionare la shared memory friends (https://linux.die.net/man/2/mremap oppure 'man 2 mremap' */
#include <sys/msg.h>    /*  per le code di messaggi. */
#include <sys/shm.h>    /*  per la memoria condivisa. */
#include <sys/signal.h> /*  per gestire i segnali. signal(), kill(). */
#include <sys/types.h>  /*  per la portabilità */
#include <sys/wait.h>   /*  per la wait() e la sleep(). */
#include <time.h>
#include <unistd.h> /*  per la fork, getpid, getppid, etc. */

#include "sem.h"

/* FOREGROUND COLOR */
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

/* BACKGROUND COLOR */
#define BLACK_BKG "\033[40m"
#define RED_BKG "\033[41m"
#define GREEN_BKG "\033[42m"
#define YELLOW_BKG "\033[43m"
#define BLUE_BKG "\033[44m"
#define MAGENTA_BKG "\033[45m"
#define CYAN_BKG "\033[46m"
#define WHITE_BKG "\033[47m"

#define specialSender (-1)

/**
 *  »»»»»»»»»» COSTANTI di Configurazione »»»»»»»»»»
 **/
#define SO_BLOCK_SIZE 4     /* numero di transazioni massime presenti in un blocco del libro mastro */
#define SO_REGISTRY_SIZE 20 /* numero di blocchi massimi presenti nel libro mastro */
#define TOO_MANY_USERS 50
#define TOO_MANY_NODES 50

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
int SO_NUM_FRIENDS;          /* IMPORTANTE numero di nodi amici dei processi nodo (solo per la versione full) */
int SO_HOPS;                 /* IMPORTANTE numero massimo di salti massimi che una transazione può effettuare quando la transaction pool di un nodo è piena (solo per la versione full) */

/**
 * »»»»»»»»»» VARIABILI »»»»»»»»»»
 **/
int offset; /* offset per le memorie condivise 'users'/'nodes', ogni utente/nodo è a conoscenza della sua posizione in 'users'/'nodes' */
int try;    /* numero di volte che un utente ha fallito l'invio di una transazione  */

int semId;            /* ftok(..., 's') => 's': semaphore */
int shmLedgerId;      /* ftok(..., 'l') => 'l': ledger */
int shmUsersId;       /* ftok(..., 'u') => 'u': users */
int shmNodesId;       /* ftok(..., 'n') => 'n': nodes */
int shmFriendsId;     /* ftok(..., 'z') => 'z': friends */
int shmActiveUsersId; /* ftok(..., 'a') => 'a': active user process */
int shmActiveNodesId; /* ftok(..., 'g') => 'g': active node process */
int messageQueueId;   /* ftok(..., 'q') => 'q': message queue */
int responseQueueId;  /* ftok(..., 'r') => 'r': response queue */
int friendsQueueId;   /* ftok(..., 'f') => 'f': friends queue */

/**
 * »»»»»»»»»» STRUTTURE DATI »»»»»»»»»»
 */

struct sigaction act;
struct timespec tp;
struct timespec remaining, request;

typedef struct _transaction {
    unsigned long timestamp; /* timestamp della transazione con risoluzione dei nanosecondi */
    pid_t sender;            /* (implicito, in quanto è l’utente che ha generato la transazione) */
    pid_t receiver;          /* utente destinatario della somma */
    unsigned int quantity;   /* quantità di denaro inviata */
    unsigned int reward;     /* denaro pagato dal sender al nodo che processa la transazione */
    unsigned int hops;       /* salti rimanenti che la transazione può eseguire da un nodo ad un altro */
} transaction;

typedef struct _block {
    unsigned int size;
    transaction transaction[SO_BLOCK_SIZE];
} block;

/* »»»»»»»»»» Libro Mastro »»»»»»»»»» */
typedef struct _ledger {
    unsigned int size;
    block block[SO_REGISTRY_SIZE];
} ledger;

/* »»»»»»»»»» Processo Utente »»»»»»»»»» */
typedef struct _userProcess {
    pid_t pid;
    int balance;
    int alive;
} userProcess;

/* »»»»»»»»»» Processo Nodo »»»»»»»»»» */
typedef struct _nodeProcess {
    pid_t pid;
    int balance;
    int poolSize;
    int* friends;
    int friendNum;
} nodeProcess;

/* »»»»»»»»»» Messaggio »»»»»»»»»» */
typedef struct _message {
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
 * »»»»»»»»»» INTESTAZIONI »»»»»»»»»»
 */

/** Metodo che gestisce la stampa degli errori
 *
 *   @param txt Testo da stampare
 **/
void error(char* txt);

/** Inizializza le variabili neccessarie per l'esecuzione di un Utente o di un Nodo
 *
 * @param argv vettore di stringhe contenente i parametri da inizializzare
 **/
void initVariable(char** argv);

/* Gestore dei segnali */
void hdl(int, siginfo_t*, void*);

/* Inizializza tutte le struture IPC necessarie comuni a tutti i processi */
void initIPCs();

/** Stampa a video le informazioni di una transazione
 *
 * @param t Transazione
 * */
void printTransaction(transaction* t);

/** Stampa a video le informazioni di un blocco
 *
 * @param b Blocco
 * */
void printBlock(block* b);

/** Stampa a video le informazioni del Libro Mastro
 *
 * @param l Libro Mastro
 * */
void printLedger(ledger* l);

/** Controlla se una transazione è già stata calcolata nel bilancio in base al suo timestamp
 *
 * @param lastVisited puntatore all'ultima transazione visitata
 * @param trans puntatore alla transazione da verificare
 *
 * @return 0 se è già stata calcolata nel bilancio, -1 altrimenti
 **/
int alreadyVisited(transaction* lastVisited, transaction* trans);

/** Calcola il bilancio delle transazioni presenti nel Libro Mastro
 *
 * @param identifier pid/ID unico per riconoscimento di utente/nodo
 * @param lastVisited puntatore all'ultima transazione visitata (già contata dal libro mastro)
 *
 * @return il bilancio (entrate) delle transazioni che l'utente ha ricevuto
 */
int balanceFromLedger(pid_t identifier, transaction* lastVisited);

/* Aspetta un quanto di tempo random compreso tra MIN e MAX */
void sleepTransaction(long min, long max);

/** Stampa solo i processi con maggior e minor budget
 *
 * @param type tipo di processo: 'u' -> utente; 'n' -> nodo;
 */
void tooManyProcess(char type);

#endif