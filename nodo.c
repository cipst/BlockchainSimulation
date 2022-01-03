#include "header.h"

#define DEBUG

/** Handler dei segnali:
 *  	-	SIGINT
 *  	-	SIGTERM
 *  	-	SIGSEGV
 *      -   SIGUSR1 
 **/
void hdl(int, siginfo_t*, void*);

/** Aggiunge una transazione alla transaction pool
 * 
 * @param trans transazione da aggiungere alla transaction pool
 **/
void addTransaction(transaction);

void printPool();

transaction* pool;

int queueId; /* ftok(..., 'q') => 'q': queue */
int i, j, pos = 0;

int main(int argc, char** argv) {
    initVariable(argv);

    pool = (transaction*)malloc(sizeof(transaction) * SO_TP_SIZE); /* transaction pool che conterrà tutte le transazioni di questo nodo */

    /* Copia il carattere \0 in act per tutta la sua lunghezza */
    memset(&act, '\0', sizeof(act));

    /* Dico a sigaction (act) quale handler deve mandare in esecuzione */
    act.sa_sigaction = &hdl;

    act.sa_flags = SA_SIGINFO | SA_NODEFER;

    /* »»»»»»»»»» SEGNALI »»»»»»»»»» */
    if (sigaction(SIGINT, &act, NULL) < 0) {
        perror(RED "[NODE] Sigaction: Failed to assign SIGINT to custom handler" RESET);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGTERM, &act, NULL) < 0) {
        perror(RED "[NODE] Sigaction: Failed to assign SIGTERM to custom handler" RESET);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGSEGV, &act, NULL) < 0) {
        perror(RED "[NODE] Sigaction: Failed to assign SIGSEGV to custom handler" RESET);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGUSR1, &act, NULL) < 0) {
        perror(RED "[NODE] Sigaction: Failed to assign SIGUSR1 to custom handler" RESET);
        exit(EXIT_FAILURE);
    }

    /* »»»»»»»»»» SEMAFORI »»»»»»»»»» */
    if ((semId = semget(ftok("./utils/private-key", 's'), 6, IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[NODE] Semaphore pool creation failure" RESET);
        exit(EXIT_FAILURE);
    }

    /* »»»»»»»»»» MEMORIE CONDIVISE »»»»»»»»»» */
    if ((shmLedgerId = shmget(ftok("./utils/private-key", 'l'), sizeof(ledger), IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[NODE] Shared Memory creation failure [LEDGER]" RESET);
        exit(EXIT_FAILURE);
    }

    mastro = (ledger*)shmat(shmLedgerId, NULL, 0);
    if (mastro == (void*)-1) {
        perror(RED "[NODE] Shared Memory attach failure [LEDGER]" RESET);
        exit(EXIT_FAILURE);
    }

    if ((shmUsersId = shmget(ftok("./utils/private-key", 'u'), sizeof(userProcess) * SO_USERS_NUM, IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[NODE] Shared Memory creation failure [USERS]" RESET);
        exit(EXIT_FAILURE);
    }

    users = (userProcess*)shmat(shmUsersId, NULL, 0);
    if (users == (void*)-1) {
        perror(RED "[NODE] Shared Memory attach failure [USERS]" RESET);
        exit(EXIT_FAILURE);
    }

    if ((shmNodesId = shmget(ftok("./utils/private-key", 'n'), sizeof(nodeProcess) * SO_NODES_NUM, IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[NODE] Shared Memory creation failure [NODES]" RESET);
        exit(EXIT_FAILURE);
    }

    nodes = (nodeProcess*)shmat(shmNodesId, NULL, 0);
    if (nodes == (void*)-1) {
        perror(RED "[NODE] Shared Memory attach failure [NODES]" RESET);
        exit(EXIT_FAILURE);
    }

    /* per sincronizzare i processi nodo alla creazione iniziale, e dare loro il via con le operazioni */
    shmActiveNodesId = shmget(ftok("./utils/private-key", 'g'), sizeof(int), IPC_CREAT | 0644);
    if (shmActiveNodesId < 0) {
        perror(RED "[NODE] Shared Memory creation failure [ActiveNodes]" RESET);
        exit(EXIT_FAILURE);
    }

    activeNodes = (int*)shmat(shmActiveNodesId, NULL, 0);
    if (activeNodes == (void*)-1) {
        perror(RED "[NODE] Shared Memory attach failure [ActiveNodes]" RESET);
        exit(EXIT_FAILURE);
    }

    /* »»»»»»»»»» CODA DI MESSAGGI »»»»»»»»»» */
    if ((queueId = msgget(ftok("./utils/private-key", 'q'), IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[NODE] Message Queue creation failure" RESET);
        exit(EXIT_FAILURE);
    }

    reserveSem(semId, nodeSync);
    (*activeNodes)++;
    releaseSem(semId, nodeSync);

#ifdef DEBUG
    reserveSem(semId, print);
    printf("[%s%d%s] NODE - Wrote +1 in shared\n", BLUE, getpid(), RESET);
    releaseSem(semId, print);
#endif

    srand(time(NULL) % getpid()); /* randomize seed */

    reserveSem(semId, nodeShm);
    (nodes + offset)->pid = getpid(); /* salvo il PID del processo in esecuzione nella memoria condivisa con la lista dei nodi */
    (nodes + offset)->balance = 0;    /* inizializzo il bilancio del nodo a 0 */
    releaseSem(semId, nodeShm);

    while (1) {
        message msg;

        if (msgrcv(queueId, &msg, sizeof(msg) - sizeof(long), getpid(), MSG_NOERROR) < 0) {
            perror(RED "[NODE] Error in msgrcv()" RESET);
            exit(EXIT_FAILURE);
        }

#ifdef DEBUG
        reserveSem(semId, print);
        printf("[ %s%d%s ] Transaction : %s(%lu, %d, %d)%s\n", BLUE, getpid(), RESET, YELLOW, msg.transaction.timestamp, msg.transaction.sender, msg.transaction.receiver, RESET);
        releaseSem(semId, print);
#endif

        /* TEST */
        /* DELETE */
        mastro->block[mastro->size].transaction[mastro->block[mastro->size].size] = msg.transaction;
        mastro->block[mastro->size].size++;
        mastro->size++;

        addTransaction(msg.transaction); 

        /* kill(msg.transaction.sender, SIGUSR2); */

        /* // pos < SO_TP_SIZE 
        if (100 < SO_TP_SIZE) {
            *(pool + pos) = msg.transaction;  // salvo la transazione nella transaction pool
            printTransaction(*(pool + pos));
            pos++;
        } else {
            reserveSem(semId, 1);
            printf("[%d] TIMESTAMP LETTO: %ld\n", getpid(), msg.transaction.timestamp);
            releaseSem(semId, 1);
            kill(msg.transaction.sender, SIGUSR2);
        } */
    }

    printf("\t%d Termino...\n", getpid());

    shmdt(mastro);
    shmdt(users);
    shmdt(nodes);
    exit(EXIT_SUCCESS);
}

void hdl(int sig, siginfo_t* siginfo, void* context) {
    /**
	 * 	Questo è l'handler dei segnali, gestisce i segnali:
	 *  	-	SIGINT
	 *  	-	SIGTERM
	 *  	-	SIGSEGV
     *      -   SIGUSR1
	 **/
    switch (sig) {
        case SIGINT:
#ifdef DEBUG
            reserveSem(semId, print);
            printPool();
            releaseSem(semId, print);
#endif
            releaseSem(semId, nodeShm);
            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            free(pool);
            exit(EXIT_SUCCESS);

        case SIGTERM:
#ifdef DEBUG
            reserveSem(semId, print);
            printPool();
            releaseSem(semId, print);
#endif
            releaseSem(semId, nodeShm);
            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            free(pool);
            exit(EXIT_FAILURE);

        case SIGSEGV:
            releaseSem(semId, nodeShm);
            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            free(pool);
            error("SEGMENTATION VIOLATION [NODE]");

        case SIGUSR1: {
            /* transaction trans;*/
            reserveSem(semId, print);
            printf("\n[ %sSIGUSR1%s ] Transaction creation due to SIGUSR1\n", YELLOW, RESET);
            releaseSem(semId, print);
            /*trans = createTransaction();
            sendTransaction(trans); */
            break;
        }
    }
}

void addTransaction(transaction trans) {
    *(pool + pos) = trans; /* salvo la transazione nella transaction pool */

    pos++; /* posizione per la prossima transazione && grandezza della transaction pool*/

    reserveSem(semId, nodeShm);
    (nodes + offset)->poolSize = pos; /* aggiorno l'attuale grandezza della transaction pool in memoria condivisa */
    releaseSem(semId, nodeShm);
}

void printPool() {
    int j;
    reserveSem(semId, print);
    printf("\n[ %s%d%s ] Transaction pool\n", BLUE, getpid(), RESET);
    for (j = 0; j < pos; j++) {
        printTransaction((pool + j));
    }
    releaseSem(semId, print);
}