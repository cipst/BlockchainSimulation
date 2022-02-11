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
            /* (users + offset)->balance += balanceFromLedger(getpid(), &lastVisited);
            releaseSem(semId, userShm); */

            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            shmdt(activeUsers);

            reserveSem(semId, print);
            printf("\n\t[ %s%d%s ] %sSIGINT%s received\n", CYAN, getpid(), RESET, YELLOW, RESET);
            releaseSem(semId, print);

            exit(EXIT_FAILURE);

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
            printf("\n[ %sSIGUSR1%s ] %s%d%s - Transaction creation due to SIGUSR1 from %s%s%d%s\n", YELLOW, RESET, CYAN, getpid(), RESET, BOLD, GREEN, siginfo->si_pid, RESET);
            releaseSem(semId, print);
            if ((users + offset)->balance >= 2) {
                trans = createTransaction();

                reserveSem(semId, print);
                printf("\t[ %s%d%s ] %s%sNEW%s", CYAN, getpid(), RESET, BOLD, YELLOW, RESET);
                printTransaction(&trans);
                printf("\n");
                releaseSem(semId, print);

                sendTransaction(&trans);
            }
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
    amount = (rand() % (((users + offset)->balance) - 1)) + 2; /* quantità da inviare: estraggo un numero compreso tra 2 e balance ==> estraggo un numero compreso tra 0 e balance-1 escluso ==> tra 0 e balance-2 e sommo 2 */

    trans.receiver = (users + userReceiver)->pid; /* imposto il destinatario della transazione */
    releaseSem(semId, userShm);

    nodeReward = (SO_REWARD * amount) / 100;        /* calcolo del reward per il nodo */
    nodeReward = (nodeReward < 1) ? 1 : nodeReward; /* nodeReward deve essere almeno 1, se è inferiore ad 1 imposto nodeReward a 1*/

    clock_gettime(CLOCK_MONOTONIC, &tp);
    trans.timestamp = (tp.tv_sec * 1000000000) + tp.tv_nsec; /* in questo modo ho creato un numero identificatore unico per questa transazione */
    trans.sender = getpid();
    trans.quantity = amount - nodeReward;
    trans.reward = nodeReward;
    trans.hops = SO_HOPS;

    return trans;
}

void sendTransaction(transaction* trans) {
    int nodesNum, nodeReceiver;
    message msg;

    reserveSem(semId, nodeSync);
    nodesNum = (*activeNodes);
    releaseSem(semId, nodeSync);

    nodeReceiver = (rand() % nodesNum); /* estraggo randomicamente una posizione tra 0 e nodesNum escluso */

    msg.mtype = (long)((nodes + nodeReceiver)->pid);
    msg.transaction = *trans;

    if (msgsnd(messageQueueId, &msg, sizeof(msg) - sizeof(long), IPC_NOWAIT) < 0) {
        perror(RED "[USER] Error in msgsnd() SEND_TRANSACTION" RESET);
        kill(getppid(), SIGUSR1);
        exit(EXIT_FAILURE);
    }

#ifdef DEBUG
    reserveSem(semId, print);
    printf("[ %s%d%s ] %s%sSENDED%s: ", CYAN, getpid(), RESET, BOLD, BLUE, RESET);
    printTransaction(trans);
    releaseSem(semId, print);
#endif
}

void receiveResponse() {
    message msg;

    if (msgrcv(responseQueueId, &msg, sizeof(msg) - sizeof(long), getpid(), IPC_NOWAIT | MSG_NOERROR) < 0) {
        if (errno != ENOMSG) {
            perror(RED "[USER] Error in responseQueueId msgrcv() RECEIVE_RESPONSE" RESET);
            exit(EXIT_FAILURE);
        } else {
            try = 0;
        }
    } else {
        printf("[ %s%d%s ] %s%sFAIL%s", CYAN, getpid(), RESET, BOLD, RED, RESET);
        printTransaction(&(msg.transaction));

        reserveSem(semId, userShm);
        (users + offset)->balance += (msg.transaction.quantity + msg.transaction.reward);
        releaseSem(semId, userShm);

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

void initUserIPC() {
    initIPCs();

    /* per sincronizzare i processi alla creazione iniziale, e dare loro il via con le operazioni */
    shmActiveUsersId = shmget(ftok("./utils/key-private", 'a'), sizeof(int), IPC_CREAT | 0644);
    if (shmActiveUsersId < 0) {
        perror(RED "Shared Memory creation failure Active Users" RESET);
        exit(EXIT_FAILURE);
    }

    activeUsers = (int*)shmat(shmActiveUsersId, NULL, 0);
    if (activeUsers == (void*)-1) {
        perror(RED "Shared Memory attach failure Active Users" RESET);
        exit(EXIT_FAILURE);
    }

    shmActiveNodesId = shmget(ftok("./utils/private-key", 'g'), sizeof(int), IPC_CREAT | 0644);
    if (shmActiveNodesId < 0) {
        perror(RED "Shared Memory creation failure Active Nodes" RESET);
        exit(EXIT_FAILURE);
    }

    activeNodes = (int*)shmat(shmActiveNodesId, NULL, 0);
    if (activeNodes == (void*)-1) {
        perror(RED "Shared Memory attach failure Active Nodes" RESET);
        exit(EXIT_FAILURE);
    }

    if ((messageQueueId = msgget(ftok("./utils/private-key", 'q'), IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "Message Queue failure" RESET);
        exit(EXIT_FAILURE);
    }

    if ((responseQueueId = msgget(ftok("./utils/private-key", 'r'), IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "Response Queue failure" RESET);
        exit(EXIT_FAILURE);
    }
}
