#include "./headers/utente.h"

/* #define DEBUG */

int i, j;

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

    initUserIPC();

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
    (users + offset)->alive = 0;                /* inizializzo lo stato dell'utente a VIVO */
    releaseSem(semId, userShm);

#ifdef DEBUG
    reserveSem(semId, print);
    printf("[ %s%d%s ] BILANCIO INIZIALE: %d\n", CYAN, getpid(), RESET, (users + offset)->balance);
    releaseSem(semId, print);
#endif

    while (1) {
        reserveSem(semId, userShm); /* aggiorno il bilancio */
        (users + offset)->balance += balanceFromLedger(getpid(), &lastVisited);
        releaseSem(semId, userShm);

#ifdef DEBUG
        reserveSem(semId, print);
        printf("[ %s%d%s ] %s%sBALANCE%s %s%d%s $\n", CYAN, getpid(), RESET, BOLD, YELLOW, RESET, BOLD, (users + offset)->balance, RESET);
        releaseSem(semId, print);
#endif

        receiveResponse();

        if (((users + offset)->balance) >= 2) {
            transaction trans;

            trans = createTransaction();

            sendTransaction(&trans);

            reserveSem(semId, print);
            printf("[ %s%d%s ] %s%sNEW%s", CYAN, getpid(), RESET, BOLD, YELLOW, RESET);
            printTransaction(&trans);
            releaseSem(semId, print);

            reserveSem(semId, userShm);
            (users + offset)->balance -= (trans.quantity + trans.reward);
            releaseSem(semId, userShm);

            sleepTransaction(SO_MIN_TRANS_GEN_NSEC, SO_MAX_TRANS_GEN_NSEC);
            /* sleep(1); */
        } else {
#ifdef DEBUG
            reserveSem(semId, print);
            printf("[ %s%d%s ] Insufficient funds: %d\n", CYAN, getpid(), RESET, (users + offset)->balance);
            releaseSem(semId, print);
            sleep(1);
#endif
        }
    }

    exit(EXIT_SUCCESS);
}