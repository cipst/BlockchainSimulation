#include "./headers/nodo.h"

/* #define DEBUG */

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

    initNodeIPC(); /* inizializzo le risorse IPC */

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
    (nodes + offset)->poolSize = 0;   /* inizializzo la grandezza della transaction pool a 0 */
    releaseSem(semId, nodeShm);

    while (1) {
        message msg;

        reserveSem(semId, nodeShm); /* aggiorno il bilancio */
        ((nodes + offset)->balance) += balanceFromLedger(getpid(), &lastVisited);
        releaseSem(semId, nodeShm);

        while (msgrcv(messageQueueId, &msg, sizeof(msg) - sizeof(long), getpid(), IPC_NOWAIT) >= 0) {
            if (addTransaction(&insertPos, msg.transaction) == -1) {
                /* impossibile aggiungere la transazione alla Transaction Pool di questo nodo */

                /* viene inviata ad un altro nodo amico */
                sendTransactionToFriend(msg.transaction);

                /* msg.mtype = (long)msg.transaction.sender;
                if (msgsnd(responseQueueId, &msg, sizeof(msg) - sizeof(long), IPC_NOWAIT) < 0) {
                    perror(RED "[NODE] Error in msgsnd()" RESET);
                    exit(EXIT_FAILURE);
                } */
            }
        }

        if (createBlock(&b, &removePos) == 0) {
            reserveSem(semId, print);
            printf("\n[ %s%d%s ] %s%sNEW BLOCK%s\n", BLUE, getpid(), RESET, BOLD, GREEN, RESET);

            printBlock(&b);
            releaseSem(semId, print);

            /* sleep(1); */
            sleepTransaction(SO_MIN_TRANS_PROC_NSEC, SO_MAX_TRANS_PROC_NSEC);

            if (updateLedger(&b) == 0) {
                reserveSem(semId, print);
                printf("\n[ %s%d%s ] %s%sADDED TO THE LEDGER%s\n", BLUE, getpid(), RESET, BOLD, YELLOW, RESET);
                releaseSem(semId, print);

                if (removeBlockFromPool(&b) == -1) {
                    reserveSem(semId, print);
                    printf("[ %s%d%s ] %sError%s while deleting the block\n", BLUE, getpid(), RESET, RED, RESET);
                    releaseSem(semId, print);

                    /* printBlock(&b); */

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
                reserveSem(semId, print);
                printf("\n[ %s%d%s ] %s%sNOT ADDED TO THE LEDGER%s\n", BLUE, getpid(), RESET, BOLD, RED, RESET);
                releaseSem(semId, print);
                kill(getppid(), SIGUSR2); /* informo il master che il Libro Mastro è pieno */
            }
        }
    }

    shmdt(mastro);
    shmdt(users);
    shmdt(nodes);

    exit(EXIT_SUCCESS);
}
