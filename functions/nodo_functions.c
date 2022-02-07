#include "../headers/nodo.h"

void hdl(int sig, siginfo_t* siginfo, void* context) {
    /**
	 * 	Questo è l'handler dei segnali, gestisce i segnali:
	 *  	-	SIGINT
	 *  	-	SIGTERM
	 *  	-	SIGSEGV
	 **/
    switch (sig) {
        case SIGINT:
            (nodes + offset)->balance += balanceFromLedger(getpid(), &lastVisited);
            releaseSem(semId, nodeShm);

            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            shmdt(activeNodes);
            free(pool);

            reserveSem(semId, print);
            printf("\n\t[ %s%d%s ] %sSIGINT%s received\n", BLUE, getpid(), RESET, YELLOW, RESET);
            releaseSem(semId, print);
            exit(EXIT_SUCCESS);

        case SIGTERM:
            (nodes + offset)->balance += balanceFromLedger(getpid(), &lastVisited);
            releaseSem(semId, nodeShm);

            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            shmdt(activeNodes);
            free(pool);

            reserveSem(semId, print);
            printf("\n\t[ %s%d%s ] %sSIGTERM%s received\n", BLUE, getpid(), RESET, YELLOW, RESET);
            releaseSem(semId, print);
            exit(EXIT_SUCCESS);

        case SIGSEGV:
            releaseSem(semId, nodeShm);
            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            shmdt(activeNodes);
            free(pool);

            error("SEGMENTATION VIOLATION [NODE]");
    }
}

int addTransaction(unsigned int* pos, transaction trans) {
    unsigned int index = (*pos) % SO_TP_SIZE; /* calcolo la posizione in modo ciclico */

    if ((pool + index)->timestamp == 0) {     /* se timestamp a 0 vuol dire che in (pool + index) non è presente alcuna transazione */
        *(pool + index) = trans;              /* salvo la transazione nella transaction pool */
        (*pos) = (((*pos) + 1) % SO_TP_SIZE); /* calcolo la prossima posizione utile */

        reserveSem(semId, nodeShm);
        (nodes + offset)->poolSize++; /* aggiorno l'attuale grandezza della transaction pool in memoria condivisa */
        releaseSem(semId, nodeShm);

        /* TEST */
        sendTransactionToFriend(trans);

#ifdef DEBUG
        reserveSem(semId, print);
        printf("[ %s%d%s ] %s%sREAD%s: ", BLUE, getpid(), RESET, BOLD, GREEN, RESET);
        printTransaction(&trans);
        releaseSem(semId, print);
#endif

        return 0;
    }
    return -1;
}

void removeTransaction(unsigned int pos) {
    memset((pool + pos), 0, sizeof(transaction)); /* cancello la transazione */

    reserveSem(semId, nodeShm);
    ((nodes + offset)->poolSize)--; /* aggiorno l'attuale grandezza della transaction pool in memoria condivisa */
    releaseSem(semId, nodeShm);
}

void sendTransactionToFriend(transaction trans) {
    message msg;
    msg.transaction = trans;

    /* controllo se la transazione può ancora saltare */
    if (SO_HOPS != 0) {
    } else {

        /* altrimenti viene inviata al master */
        msg.mtype = getppid();
        if (msgsnd(friendsQueueId, &msg, sizeof(msg) - sizeof(long), IPC_NOWAIT) < 0) {
            perror(RED "[USER] Error in msgsnd()" RESET);
            exit(EXIT_FAILURE);
        }
    }
}

int createBlock(block* b, unsigned int* pos) {
    unsigned int i, sum = 0;
    unsigned int index;
    int TPsize;

    reserveSem(semId, nodeShm);
    TPsize = (nodes + offset)->poolSize;
    releaseSem(semId, nodeShm);

    if ((TPsize >= (SO_BLOCK_SIZE - 1)) && ((b->size) < (SO_BLOCK_SIZE - 1))) {
        for (i = (*pos); b->size < (SO_BLOCK_SIZE - 1); ++i) {
            index = (i % SO_TP_SIZE);
            if ((pool + index)->timestamp != 0) { /* se timestamp è != da 0, in (pool + i) è presente una transazione */

                b->transaction[b->size] = *(pool + index); /* aggiungo la transazione al blocco */

                (b->size)++;

                sum += (pool + index)->reward;
            }
        }
        (*pos) = ((index + 1) % SO_TP_SIZE);

        clock_gettime(CLOCK_MONOTONIC, &tp);
        b->transaction[b->size].timestamp = (tp.tv_sec * 1000000000) + tp.tv_nsec; /* in questo modo ho creato un numero identificatore unico per questa transazione */
        b->transaction[b->size].sender = specialSender;
        b->transaction[b->size].receiver = getpid();
        b->transaction[b->size].quantity = sum;
        b->transaction[b->size].reward = 0;
        (b->size)++;

        return 0;
    } else {
        return -1;
    }
}

int removeBlockFromPool(block* b) {
    unsigned int i, j, index;
    int startPS, endPS, found = -1;

    reserveSem(semId, nodeShm);
    startPS = (nodes + offset)->poolSize;
    releaseSem(semId, nodeShm);

    for (i = 0; i < (b->size) - 1; ++i) {
        for (j = 0; found = -1 && j < SO_TP_SIZE; ++j) {
            index = (j % SO_TP_SIZE);
            if (b->transaction[i].timestamp == pool[index].timestamp) {       /* check timestamp */
                if (b->transaction[i].sender == pool[index].sender) {         /* check sender */
                    if (b->transaction[i].receiver == pool[index].receiver) { /* check receiver */
                        found = 0;
                        removeTransaction(index);
                    }
                }
            }
        }
    }

    reserveSem(semId, nodeShm);
    endPS = (nodes + offset)->poolSize;
    releaseSem(semId, nodeShm);

    /* controllo grandezza TP prima e dopo, se c'è qualcosa che non va return -1 */
    if ((startPS - ((b->size) - 1)) == endPS) {
        return 0;
    } else {
        return -1;
    }
}

int updateLedger(block* b) {
    reserveSem(semId, ledgerShm);

    if (mastro->size < SO_REGISTRY_SIZE) {
        mastro->block[mastro->size] = *b;
        (mastro->size)++;

        releaseSem(semId, ledgerShm);
        return 0;
    }

    releaseSem(semId, ledgerShm);
    return -1;
}

void initNodeIPC() {
    initIPCs();

    /* per sincronizzare i processi alla creazione iniziale, e dare loro il via con le operazioni */
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

    if ((friendsQueueId = msgget(ftok("./utils/private-key", 'f'), IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "Friends Queue failure" RESET);
        exit(EXIT_FAILURE);
    }
}

void printPool() {
    int j;
    reserveSem(semId, print);
    printf("\n[ %s%d%s ] Transaction pool\n", BLUE, getpid(), RESET);
    for (j = 0; j < SO_TP_SIZE; ++j) {
        printTransaction(pool + j);
    }
    releaseSem(semId, print);
}