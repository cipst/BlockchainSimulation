#include "../header.h"

void hdl(int sig, siginfo_t* siginfo, void* context) {
    /** Segnali gestiti:
    *  	-	SIGINT
    *  	-	SIGTERM
    *  	-	SIGSEGV
    *   -   SIGUSR1 (crea una nuova transazione)
    *   -   SIGUSR2 (invio transazione fallita)
    **/

    switch (sig) {
        case SIGINT:
            releaseSem(semId, userShm);
            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            shmdt(activeUsers);
            kill(getppid(), SIGUSR1);
            exit(EXIT_SUCCESS);

        case SIGTERM:
            releaseSem(semId, userShm);
            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            shmdt(activeUsers);
            kill(getppid(), SIGUSR1);
            exit(EXIT_FAILURE);

        case SIGSEGV:
            releaseSem(semId, userShm);
            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            shmdt(activeUsers);
            kill(getppid(), SIGUSR1);
            perror("");
            error("SEGMENTATION VIOLATION [USER]");

        case SIGUSR1: {
            transaction trans;
            reserveSem(semId, print);
            printf("\n[ %sSIGUSR1%s ] Transaction creation due to SIGUSR1\n", YELLOW, RESET);
            releaseSem(semId, print);
            trans = createTransaction();
            sendTransaction(&trans);
            break;
        }

        case SIGUSR2: {
            reserveSem(semId, print);
            printf("\n[ %sSIGUSR2%s ] Impossible to send transaction to %d\n", YELLOW, RESET, siginfo->si_pid);
            releaseSem(semId, print);

            /* try++; */

            /* if (try == SO_RETRY) { 
                reserveSem(semId, print);
                printf("[ %s%d%s ] Failed to send transaction for SO_RETRY times\n", CYAN, getpid(), RESET);
                releaseSem(semId, print);

                shmdt(mastro);
                shmdt(users);
                shmdt(nodes);
                shmdt(activeUsers);

                exit(EXIT_FAILURE);
            } */
            break;
        }
    }
}

void sendTransaction(transaction* trans) {
    int nodeReceiver;
    message msg;

    nodeReceiver = (rand() % SO_NODES_NUM); /* estraggo randomicamente una posizione tra 0 e SO_NODES_NUM escluso */

    msg.mtype = (long)((nodes + nodeReceiver)->pid);
    msg.transaction = *trans;

    /* reserveSem(semId, print);
    printf("\tQueueId: %d\n", queueId);
    printf("\tMSG MTYPE: %d\n", msg.mtype);
    printf("\tMSG TRANSACTION: (%lu, %d, %d)\n", msg.transaction.timestamp, msg.transaction.sender, msg.transaction.receiver);
    releaseSem(semId, print); */

    if (msgsnd(queueId, &msg, sizeof(msg) - sizeof(long), IPC_NOWAIT) < 0) {
        perror(RED "[USER] Error in msgsnd()" RESET);
        kill(getppid(), SIGUSR1);
        exit(EXIT_FAILURE);
    }

#ifdef DEBUG
    reserveSem(semId, print);
    printf("[ %s%d%s ] Transaction %s%ssended%s: (%lu, %d, %d)\n", CYAN, getpid(), RESET, BOLD, BLUE, RESET, trans->timestamp, trans->sender, trans->receiver);
    releaseSem(semId, print);
#endif
}

transaction createTransaction() {
    transaction trans;
    int userReceiver, amount, nodeReward;

    do {                                        /* estrazione processo utente destinatario */
        userReceiver = (rand() % SO_USERS_NUM); /* estraggo randomicamente una posizione tra 0 e SO_USERS_NUM escluso */
    } while (userReceiver == offset);           /* cerco la posizione random finché non è diversa dalla posizione dell'utente corrente */

    reserveSem(semId, userShm);
    amount = (rand() % ((*balance) - 1)) + 2; /* quantità da inviare: estraggo un numero compreso tra 2 e balance ==> estraggo un numero compreso tra 0 e balance-1 escluso ==> tra 0 e balance-2 e sommo 2 */

    trans.receiver = (users + userReceiver)->pid; /* imposto il destinatario della transazione */
    releaseSem(semId, userShm);

    nodeReward = (SO_REWARD * amount) / 100;        /* calcolo del reward per il nodo */
    nodeReward = (nodeReward < 1) ? 1 : nodeReward; /* nodeReward deve essere almeno 1, se è inferiore ad 1 imposto nodeReward a 1*/

    clock_gettime(CLOCK_MONOTONIC, &tp);
    trans.timestamp = (tp.tv_sec * 1000000000) + tp.tv_nsec; /* in questo modo ho creato un numero identificatore unico per questa transazione */
    trans.sender = getpid();
    trans.quantity = amount - nodeReward;
    trans.reward = nodeReward;

    /* #ifdef DEBUG
    reserveSem(semId, print);
    printTransaction(trans);
    releaseSem(semId, print);
#endif */

    return trans;
}

void sleepTransactionGen() {
    long transGenNsec, transGenSec;

    transGenNsec = (rand() % (SO_MAX_TRANS_GEN_NSEC - SO_MIN_TRANS_GEN_NSEC + 1)) + SO_MIN_TRANS_GEN_NSEC;

    if (transGenNsec > 999999999) {
        transGenSec = transGenNsec / 1000000000;    /* trovo la parte in secondi */
        transGenNsec -= (transGenSec * 1000000000); /* tolgo la parte in secondi dai nanosecondi */
    }

#ifdef DEBUG
    reserveSem(semId, print);
    printf("%sSleep for: %ld s   %ld nsec%s\n", RED, transGenSec, transGenNsec, RESET);
    releaseSem(semId, print);
#endif

    request.tv_nsec = transGenNsec;
    request.tv_sec = transGenSec;
    remaining.tv_nsec = transGenNsec;
    remaining.tv_sec = transGenSec;

    if (nanosleep(&request, &remaining) == -1) {
        switch (errno) {
            case EINTR: /* errno == EINTR quando nanosleep() viene interrotta da un gestore di segnali */
                break;

            case EINVAL:
                perror(RED "tv_nsec - not in range or tv_sec is negative" RESET);
                exit(EXIT_FAILURE);

            default:
                perror(RED "[USER] Nanosleep fail" RESET);
                exit(EXIT_FAILURE);
        }
    }
}