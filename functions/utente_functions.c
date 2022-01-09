#include "../headers/utente.h"

void hdl(int sig, siginfo_t* siginfo, void* context) {
    /** Segnali gestiti:
    *  	-	SIGINT
    *  	-	SIGTERM
    *  	-	SIGSEGV
    *   -   SIGUSR1 (crea una nuova transazione)
    **/

    switch (sig) {
        case SIGINT:
            (users + offset)->balance += balanceFromLedger(getpid(), &lastVisited);
            releaseSem(semId, userShm);

            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            shmdt(activeUsers);

            reserveSem(semId, print);
            printf("\n\t[ %s%d%s ] %sSIGINT%s received\n", CYAN, getpid(), RESET, YELLOW, RESET);
            releaseSem(semId, print);
            exit(EXIT_SUCCESS);

        case SIGTERM:
            (users + offset)->balance += balanceFromLedger(getpid(), &lastVisited);
            releaseSem(semId, userShm);
            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            shmdt(activeUsers);

            reserveSem(semId, print);
            printf("\n\t[ %s%d%s ] %sSIGTERM%s received\n", CYAN, getpid(), RESET, YELLOW, RESET);
            releaseSem(semId, print);
            exit(EXIT_SUCCESS);

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
    }
}

int isAlive(int offset) {
    int isAlive;

    reserveSem(semId, userShm);
    isAlive = (users + offset)->alive;
    releaseSem(semId, userShm);

    return isAlive;
}

transaction createTransaction() {
    transaction trans;
    int userReceiver, amount, nodeReward;

    do {                                                             /* estrazione processo utente destinatario */
        userReceiver = (rand() % SO_USERS_NUM);                      /* estraggo randomicamente una posizione tra 0 e SO_USERS_NUM escluso */
    } while (userReceiver == offset || isAlive(userReceiver) == -1); /* cerco la posizione random finché non è diversa dalla posizione dell'utente corrente e finchè non trovo un utente ancora vivo */

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

    return trans;
}

void sendTransaction(transaction* trans) {
    int nodeReceiver;
    message msg;

    nodeReceiver = (rand() % SO_NODES_NUM); /* estraggo randomicamente una posizione tra 0 e SO_NODES_NUM escluso */

    msg.mtype = (long)((nodes + nodeReceiver)->pid);
    msg.transaction = *trans;

    if (msgsnd(messageQueueId, &msg, sizeof(msg) - sizeof(long), IPC_NOWAIT) < 0) {
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

void receiveResponse() {
    message msg;

    if (msgrcv(responseQueueId, &msg, sizeof(msg) - sizeof(long), getpid(), IPC_NOWAIT | MSG_NOERROR) < 0) {
        if (errno != ENOMSG) {
            perror(RED "[USER] Error in responseQueueId msgrcv()" RESET);
            exit(EXIT_FAILURE);
        } else {
            try = 0;
        }
    } else {
        printf("[ %s%d%s ] %s%sFAIL%s", CYAN, getpid(), RESET, BOLD, RED, RESET);
        printTransaction(&(msg.transaction));

        try++;

        if (try == SO_RETRY) {
            reserveSem(semId, print);
            printf("[ %s%d%s ] Failed to send transaction for %d (SO_RETRY) times\n", CYAN, getpid(), RESET, SO_RETRY);
            releaseSem(semId, print);

            reserveSem(semId, userShm);
            (users + offset)->alive = -1; /* imposto lo stato dell'utente a MORTO */
            releaseSem(semId, userShm);

            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            shmdt(activeUsers);
            shmdt(activeNodes);

            kill(getppid(), SIGUSR1);

            exit(EXIT_FAILURE);
        }
    }
}