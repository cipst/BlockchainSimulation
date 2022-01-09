#include "./headers/nodo.h"

#define DEBUG

block b; /* blocco candidato */

int i, j;
unsigned int insertPos = 0;
unsigned int removePos = 0;

int main(int argc, char** argv) {
    initVariable(argv);

    pool = (transaction*)calloc(SO_TP_SIZE, sizeof(transaction));
    b.size = 0; /* inizializzo la grandezza del blocco candidato a 0 */

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

    initIPCs('n'); /* inizializzo le risorse IPC */

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
    (nodes + offset)->pid = getpid();       /* salvo il PID del processo in esecuzione nella memoria condivisa con la lista dei nodi */
    (nodes + offset)->balance = 0;          /* inizializzo il bilancio del nodo a 0 */
    (nodes + offset)->poolSize = 0;         /* inizializzo la grandezza della transaction pool a 0 */
    balance = &((nodes + offset)->balance); /* RIFERIMENTO al bilancio */
    releaseSem(semId, nodeShm);

    while (1) {
        message msg;

        reserveSem(semId, nodeShm); /* aggiorno il bilancio */
        (*balance) += balanceFromLedger(mastro, getpid(), &lastVisited);
        releaseSem(semId, nodeShm);

        while (msgrcv(messageQueueId, &msg, sizeof(msg) - sizeof(long), getpid(), IPC_NOWAIT) >= 0) {
            if (addTransaction(&insertPos, msg.transaction) == -1) {
                msg.mtype = (long)msg.transaction.sender;
                if (msgsnd(responseQueueId, &msg, sizeof(msg) - sizeof(long), IPC_NOWAIT) < 0) {
                    perror(RED "[NODE] Error in msgsnd()" RESET);
                    exit(EXIT_FAILURE);
                }
            }
        }

        if (createBlock(&b, &removePos) == 0) {
            reserveSem(semId, print);
            printf("\n[ %s%d%s ] Block created\n", BLUE, getpid(), RESET);

            printBlock(&b);
            releaseSem(semId, print);

            sleepTransaction(SO_MIN_TRANS_PROC_NSEC, SO_MAX_TRANS_PROC_NSEC);

            if (updateLedger(&b) == 0) {
                if (removeBlockFromPool(&b) == -1) {
                    reserveSem(semId, print);
                    printf("[ %s%d%s ] Error while deleting the block\n", BLUE, getpid(), RESET);
                    releaseSem(semId, print);

                    printBlock(&b);

                    shmdt(users);
                    shmdt(nodes);
                    shmdt(mastro);
                    shmdt(activeNodes);
                    msgctl(messageQueueId, IPC_RMID, NULL);

                    exit(EXIT_FAILURE);
                }

                /* svuoto il blocco */
                memset(&b, 0, sizeof(block));
            } else {
                kill(getppid(), SIGUSR2); /* informo il master che il Libro Mastro è pieno */
            }
        }

        sleep(2);
    }

    shmdt(mastro);
    shmdt(users);
    shmdt(nodes);

    exit(EXIT_SUCCESS);
}
