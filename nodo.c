#include "master.h"

/* #define DEBUG */

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

struct sigaction act;

int semId;            /* ftok(..., 's') => 's': semaphore */
int queueId;          /* ftok(..., 'q') => 'q': queue */
int shmLedgerId;      /* ftok(..., 'l') => 'l': ledger */
int shmUsersId;       /* ftok(..., 'u') => 'u': users */
int shmNodesId;       /* ftok(..., 'n') => 'n': nodes */
int shmActiveNodesId; /* ftok(..., 'g') => 'g': active user process */

ledger* mastro;     /* informazioni sul libro mastro in memoria condivisa */
userProcess* users; /* informazioni sugli utenti in memoria condivisa */
nodeProcess* nodes; /* informazione sui nodi in memoria condivisa */
int* activeNodes;   /* conteggio dei processi nodo attivi in shared memory */

int SO_USERS_NUM;            /* numero di utenti nella simulazione */
int SO_NODES_NUM;            /* numero di nodi nella simulazione */
int SO_BUDGET_INIT;          /* budget iniziale di un utente */
int SO_TP_SIZE;              /* SO_TP_SIZE massima lunghezza della transaction pool */
long SO_MIN_TRANS_PROC_NSEC; /* minimo valore del tempo simulato (espresso in nanosecondi) di processamento di un blocco da parte di un nodo */
long SO_MAX_TRANS_PROC_NSEC; /* massimo valore del tempo simulato (espresso in nanosecondi) di processamento di un blocco da parte di un nodo */
int SO_RETRY;                /* numero massimo di fallimenti consecutivi nella generazione di transazioni dopo cui un processo utente termina */
int offset;                  /* offset per la memoria condivisa 'users', ogni utente è a conoscenza della sua posizione in 'users' */

transaction* pool;

int balance = 0;
int i, j, pos = 0;

int main(int argc, char** argv) {
    /* Salvo le info necessarie per il nodo */
    SO_USERS_NUM = atoi(argv[1]);
    SO_NODES_NUM = atoi(argv[2]);
    SO_BUDGET_INIT = atoi(argv[3]);
    SO_TP_SIZE = atoi(argv[4]);
    SO_MIN_TRANS_PROC_NSEC = atol(argv[5]);
    SO_MAX_TRANS_PROC_NSEC = atol(argv[6]);
    SO_RETRY = atoi(argv[7]);
    offset = atoi(argv[8]);

    pool = (transaction*)malloc(sizeof(transaction) * SO_TP_SIZE); /* transaction pool che conterrà tutte le transazioni di questo nodo */

    /* Copia il carattere \0 in act per tutta la sua lunghezza */
    memset(&act, '\0', sizeof(act));

    /* Dico a sigaction (act) quale handler deve mandare in esecuzione */
    act.sa_sigaction = &hdl;

    act.sa_flags = SA_SIGINFO | SA_NODEFER;

    /* »»»»»»»»»» SEGNALI »»»»»»»»»» */
    if (sigaction(SIGINT, &act, NULL) < 0) {
        perror(RED "[NODE] Sigaction: Failed to assign SIGINT to custom handler" WHITE);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGTERM, &act, NULL) < 0) {
        perror(RED "[NODE] Sigaction: Failed to assign SIGTERM to custom handler" WHITE);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGSEGV, &act, NULL) < 0) {
        perror(RED "[NODE] Sigaction: Failed to assign SIGSEGV to custom handler" WHITE);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGUSR1, &act, NULL) < 0) {
        perror(RED "[NODE] Sigaction: Failed to assign SIGUSR1 to custom handler" WHITE);
        exit(EXIT_FAILURE);
    }

    /* »»»»»»»»»» SEMAFORI »»»»»»»»»» */
    if ((semId = semget(ftok("./utils/private-key", 's'), 3, IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[NODE] Semaphore pool creation failure" WHITE);
        exit(EXIT_FAILURE);
    }

    /* »»»»»»»»»» MEMORIE CONDIVISE »»»»»»»»»» */
    if ((shmLedgerId = shmget(ftok("./utils/private-key", 'l'), sizeof(ledger), IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[NODE] Shared Memory creation failure [LEDGER]" WHITE);
        exit(EXIT_FAILURE);
    }

    mastro = (ledger*)shmat(shmLedgerId, NULL, 0);
    if (mastro == (void*)-1) {
        perror(RED "[NODE] Shared Memory attach failure [LEDGER]" WHITE);
        exit(EXIT_FAILURE);
    }

    if ((shmUsersId = shmget(ftok("./utils/private-key", 'u'), sizeof(userProcess) * SO_USERS_NUM, IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[NODE] Shared Memory creation failure [USERS]" WHITE);
        exit(EXIT_FAILURE);
    }

    users = (userProcess*)shmat(shmUsersId, NULL, 0);
    if (users == (void*)-1) {
        perror(RED "[NODE] Shared Memory attach failure [USERS]" WHITE);
        exit(EXIT_FAILURE);
    }

    if ((shmNodesId = shmget(ftok("./utils/private-key", 'n'), sizeof(nodeProcess) * SO_NODES_NUM, IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[NODE] Shared Memory creation failure [NODES]" WHITE);
        exit(EXIT_FAILURE);
    }

    nodes = (nodeProcess*)shmat(shmNodesId, NULL, 0);
    if (nodes == (void*)-1) {
        perror(RED "[NODE] Shared Memory attach failure [NODES]" WHITE);
        exit(EXIT_FAILURE);
    }

    /* per sincronizzare i processi nodo alla creazione iniziale, e dare loro il via con le operazioni */
    shmActiveNodesId = shmget(ftok("./utils/private-key", 'g'), sizeof(int), IPC_CREAT | 0644);
    if (shmActiveNodesId < 0) {
        perror(RED "[NODE] Shared Memory creation failure [ActiveNodes]" WHITE);
        exit(EXIT_FAILURE);
    }

    activeNodes = (int*)shmat(shmActiveNodesId, NULL, 0);
    if (activeNodes == (void*)-1) {
        perror(RED "[NODE] Shared Memory attach failure [ActiveNodes]" WHITE);
        exit(EXIT_FAILURE);
    }

    /* »»»»»»»»»» CODA DI MESSAGGI »»»»»»»»»» */
    if ((queueId = msgget(ftok("./utils/private-key", 'q'), IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[NODE] Message Queue creation failure" WHITE);
        exit(EXIT_FAILURE);
    }

    reserveSem(semId, 2);
    (*activeNodes)++;
    releaseSem(semId, 2);

    srand(time(NULL) % getpid()); /* randomize seed */

    while (1) {
        message msg;

        if (msgrcv(queueId, &msg, sizeof(msg) - sizeof(pid_t), getpid(), MSG_NOERROR) < 0) {
            perror(RED "[NODE] Error in msgrcv()" WHITE);
            exit(EXIT_FAILURE);
        }
        reserveSem(semId, 1);
        printf("[%d] PID LETTO: %s%d%s\n", getpid(), CYAN, msg.transaction.sender, WHITE);
        releaseSem(semId, 1);

        /* TEST */
        /* DELETE */
        mastro->block[mastro->size].transaction[mastro->block[mastro->size].size] = msg.transaction;
        mastro->block[mastro->size].size++;
        mastro->size++;

        addTransaction(msg.transaction);

        /*         kill(msg.transaction.sender, SIGUSR2); */

#ifdef DEBUG
        /* pos < SO_TP_SIZE */
        if (100 < SO_TP_SIZE) {
            *(pool + pos) = msg.transaction; /* salvo la transazione nella transaction pool */
            printTransaction(*(pool + pos));
            pos++;
        } else {
            reserveSem(semId, 1);
            printf("[%d] TIMESTAMP LETTO: %ld\n", getpid(), msg.transaction.timestamp);
            releaseSem(semId, 1);
            kill(msg.transaction.sender, SIGUSR2);
        }
#endif
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
            printPool();
            releaseSem(semId, 0);
            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            free(pool);
            exit(EXIT_SUCCESS);

        case SIGTERM:
            printPool();
            releaseSem(semId, 0);
            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            free(pool);
            exit(EXIT_FAILURE);

        case SIGSEGV:
            releaseSem(semId, 0);
            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            free(pool);
            error("SEGMENTATION VIOLATION [NODE]");

        case SIGUSR1: {
            /* transaction trans;*/
            reserveSem(semId, 1);
            printf("\n[ %sSIGUSR1%s ] Transaction creation due to SIGUSR1\n", YELLOW, WHITE);
            releaseSem(semId, 1);
            /*trans = createTransaction();
            sendTransaction(trans); */
            break;
        }
    }
}

void addTransaction(transaction trans) {
    reserveSem(semId, 1);
    printf("TRANSAZIONE AGGIUNTA\n");
    releaseSem(semId, 1);

    *(pool + pos) = trans; /* salvo la transazione nella transaction pool */

    reserveSem(semId, 1);
    printf("%d) SENDER: %d\n", pos, (pool + pos)->receiver);
    releaseSem(semId, 1);

    pos++; /* posizione per la prossima transazione && grandezza della transaction pool*/

    reserveSem(semId, 0);
    (nodes + offset)->poolSize = pos; /* aggiorno l'attuale grandezza della transaction pool in memoria condivisa */
    releaseSem(semId, 0);
}

void printPool() {
    int j;
    reserveSem(semId, 1);
    printf("\n[ %s%d%s ] Transaction pool\n", BLUE, getpid(), WHITE);
    for (j = 0; j < pos; j++) {
        /* transaction t = (pool + j)->sender; */
        printTransaction(*(pool + j));
    }
    releaseSem(semId, 1);
}