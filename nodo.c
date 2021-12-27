#include "master.h"

/* #define DEBUG */

/* Gestore dei segnali */
void hdl(int, siginfo_t*, void*);

struct sigaction act;

int semId;            /* ftok(..., 's') => 's': semaphore */
int queueId;          /* ftok(..., 'q') => 'q': queue */
int shmLedgerId;      /* ftok(..., 'l') => 'l': ledger */
int shmUsersId;       /* ftok(..., 'u') => 'u': users */
int shmNodesId;       /* ftok(..., 'n') => 'n': nodes */
int shmActiveNodesId; /* ftok(..., 'g') => 'g': active user process */

ledger* mastro;   /* informazioni sul libro mastro in memoria condivisa */
process* users;   /* informazioni sugli utenti in memoria condivisa */
process* nodes;   /* informazione sui nodi in memoria condivisa */
int* activeNodes; /* conteggio dei processi nodo attivi in shared memory */

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

    pool = (transaction*)malloc(SO_TP_SIZE); /* transaction pool che conterrà tutte le transazioni di questo nodo */

    bzero(&act, sizeof(act)); /* bzero setta tutti i bytes a zero */

    /* Dico a sigaction (act) quale handler deve mandare in esecuzione */
    act.sa_sigaction = &hdl;
    /* Il campo SA_SIGINFO flag indica a sigaction() di usare il capo sa_sigaction, e non sa_handler. */
    act.sa_flags = SA_SIGINFO;

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

    if ((shmUsersId = shmget(ftok("./utils/private-key", 'u'), sizeof(process) * SO_USERS_NUM, IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[NODE] Shared Memory creation failure [USERS]" WHITE);
        exit(EXIT_FAILURE);
    }

    users = (process*)shmat(shmUsersId, NULL, 0);
    if (users == (void*)-1) {
        perror(RED "[NODE] Shared Memory attach failure [USERS]" WHITE);
        exit(EXIT_FAILURE);
    }

    if ((shmNodesId = shmget(ftok("./utils/private-key", 'n'), sizeof(process) * SO_NODES_NUM, IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[NODE] Shared Memory creation failure [NODES]" WHITE);
        exit(EXIT_FAILURE);
    }

    nodes = (process*)shmat(shmNodesId, NULL, 0);
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

#ifdef DEBUG
        reserveSem(semId, 1);
        printf("[%d] PID LETTO: %d\n", getpid(), msg.transaction.sender);
        sleep(1);
        releaseSem(semId, 1);
#endif

        if (100 < SO_TP_SIZE) {
            *(pool + pos) = msg.transaction; /* salvo la transazione nella transaction pool */
            printTransaction(*(pool + pos));
            pos++;
        } else {
            reserveSem(semId, 1);
            printf("[%d] PID LETTO: %d\n", getpid(), msg.transaction.sender);
            releaseSem(semId, 1);
            kill(msg.transaction.sender, SIGUSR2);
        }
    }

    printf("\t%d Termino...\n", getpid());

    free(pool);
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
            releaseSem(semId, 0);
            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            exit(EXIT_SUCCESS);

        case SIGTERM:
            releaseSem(semId, 0);
            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            exit(EXIT_FAILURE);

        case SIGSEGV:
            releaseSem(semId, 0);
            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
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