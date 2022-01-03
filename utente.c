#include "header.h"

#define DEBUG

int i, j, try = 0;

int main(int argc, char** argv) {
    initVariable(argv);

    /* Copia il carattere \0 in act per tutta la sua lunghezza */
    memset(&act, '\0', sizeof(act));

    /* Dico a sigaction (act) quale handler deve mandare in esecuzione */
    act.sa_sigaction = &hdl;

    act.sa_flags = SA_SIGINFO | SA_NODEFER;

    /* »»»»»»»»»» SEGNALI »»»»»»»»»» */
    if (sigaction(SIGINT, &act, NULL) < 0) {
        perror(RED "[USER] Sigaction: Failed to assign SIGINT to custom handler" RESET);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGTERM, &act, NULL) < 0) {
        perror(RED "[USER] Sigaction: Failed to assign SIGTERM to custom handler" RESET);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGSEGV, &act, NULL) < 0) {
        perror(RED "[USER] Sigaction: Failed to assign SIGSEGV to custom handler" RESET);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGUSR1, &act, NULL) < 0) {
        perror(RED "[USER] Sigaction: Failed to assign SIGUSR1 to custom handler" RESET);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGUSR2, &act, NULL) < 0) {
        perror(RED "[USER] Sigaction: Failed to assign SIGUSR2 to custom handler" RESET);
        exit(EXIT_FAILURE);
    }

    /* »»»»»»»»»» SEMAFORI »»»»»»»»»» */
    if ((semId = semget(ftok("./utils/private-key", 's'), 6, IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[USER] Semaphore pool creation failure" RESET);
        exit(EXIT_FAILURE);
    }

    /* »»»»»»»»»» MEMORIE CONDIVISE »»»»»»»»»» */
    if ((shmLedgerId = shmget(ftok("./utils/private-key", 'l'), sizeof(ledger), IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[USER] Shared Memory creation failure [LEDGER]" RESET);
        exit(EXIT_FAILURE);
    }

    mastro = (ledger*)shmat(shmLedgerId, NULL, 0);
    if (mastro == (void*)-1) {
        perror(RED "[USER] Shared Memory attach failure [LEDGER]" RESET);
        exit(EXIT_FAILURE);
    }

    if ((shmUsersId = shmget(ftok("./utils/private-key", 'u'), sizeof(userProcess) * SO_USERS_NUM, IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[USER] Shared Memory creation failure [USERS]" RESET);
        exit(EXIT_FAILURE);
    }

    users = (userProcess*)shmat(shmUsersId, NULL, 0);
    if (users == (void*)-1) {
        perror(RED "[USER] Shared Memory attach failure [USERS]" RESET);
        exit(EXIT_FAILURE);
    }

    if ((shmNodesId = shmget(ftok("./utils/private-key", 'n'), sizeof(nodeProcess) * SO_NODES_NUM, IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[USER] Shared Memory creation failure [NODES]" RESET);
        exit(EXIT_FAILURE);
    }

    nodes = (nodeProcess*)shmat(shmNodesId, NULL, 0);
    if (nodes == (void*)-1) {
        perror(RED "[USER] Shared Memory attach failure [NODES]" RESET);
        exit(EXIT_FAILURE);
    }

    /* per sincronizzare i processi alla creazione iniziale, e dare loro il via con le operazioni */
    shmActiveUsersId = shmget(ftok("./utils/key-private", 'a'), sizeof(int), IPC_CREAT | 0644);
    if (shmActiveUsersId < 0) {
        perror(RED "[USER] Shared Memory creation failure [ActiveProcess]" RESET);
        exit(EXIT_FAILURE);
    }

    activeUsers = (int*)shmat(shmActiveUsersId, NULL, 0);
    if (activeUsers == (void*)-1) {
        perror(RED "[USER] Shared Memory attach failure [ActiveProcess]" RESET);
        exit(EXIT_FAILURE);
    }

    /* »»»»»»»»»» CODA DI MESSAGGI »»»»»»»»»» */
    /* in base al nodo estratto (nodeReceiver) scelgo la coda di messaggi di quel nodo */
    if ((queueId = msgget(ftok("./utils/private-key", 'q'), IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[USER] Message Queue failure" RESET);
        exit(EXIT_FAILURE);
    }

    /* Il semaforo 2 viene utilizzato per accedere in mutua esclusione all'area di shared memory per sincronizzarsi */
    reserveSem(semId, userSync);
    (*activeUsers)++;
    releaseSem(semId, userSync);

#ifdef DEBUG
    reserveSem(semId, print);
    printf("[%s%d%s] USER - Wrote +1 in shared\n", CYAN, getpid(), RESET);
    releaseSem(semId, print);
#endif

    srand(time(NULL) % getpid()); /* randomize seed */

    reserveSem(semId, userShm);
    (users + offset)->pid = getpid();           /* salvo il PID del processo in esecuzione nella memoria condivisa con la lista degli utenti */
    (users + offset)->balance = SO_BUDGET_INIT; /* inizializzo il bilancio a SO_BUDGET_INIT */
    balance = &((users + offset)->balance);     /* RIFERIMENTO al bilancio */
    releaseSem(semId, userShm);

#ifdef DEBUG
    reserveSem(semId, print);
    printf("[ %s%d%s ] BILANCIO INIZIALE: %d\n", CYAN, getpid(), RESET, *balance);
    releaseSem(semId, print);
#endif

    while (1) {
        reserveSem(semId, userShm); /* aggiorno il bilancio */
        (*balance) += balanceFromLedger(mastro, getpid(), &lastVisited);
        releaseSem(semId, userShm);

#ifdef DEBUG
        reserveSem(semId, print);
        printf("[ %s%d%s ] BILANCIO: %d\n", CYAN, getpid(), RESET, *balance);
        releaseSem(semId, print);
#endif

        if ((*balance) >= 2) {
            transaction trans;

            trans = createTransaction();

            sendTransaction(&trans);

#ifdef DEBUG
            reserveSem(semId, print);
            printf("[ %s%d%s ] BILANCIO PRIMA: %d\n", CYAN, getpid(), RESET, *balance);
            releaseSem(semId, print);
#endif

            reserveSem(semId, print);
            printTransaction(&trans);
            releaseSem(semId, print);

            reserveSem(semId, userShm);
            (*balance) -= (trans.quantity + trans.reward);
            releaseSem(semId, userShm);

#ifdef DEBUG
            reserveSem(semId, print);
            printf("[ %s%d%s ] BILANCIO DOPO: %d\n", CYAN, getpid(), RESET, *balance);
            releaseSem(semId, print);
#endif

            sleep(5);
            /* sleepTransactionGen(); */
        } else {
            /* TEST */
            /* printf("NOTIFICO IL MASTER CHE NON HO PIU' SOLDI\n"); */
            kill(getppid(), SIGUSR1);
            exit(EXIT_FAILURE);
        }
    }

    printf("\t%d Termino...\n", getpid());

    exit(EXIT_SUCCESS);
}