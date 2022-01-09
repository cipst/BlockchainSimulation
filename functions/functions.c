#include "../headers/header.h"

int initSemAvailable(int semId, int semNum) {
    union semun arg;
    arg.val = 1;
    return semctl(semId, semNum, SETVAL, arg);
}

int initSemInUse(int semId, int semNum) {
    union semun arg;
    arg.val = 0;
    return semctl(semId, semNum, SETVAL, arg);
}

int reserveSem(int semId, int semNum) {
    struct sembuf sops;
    sops.sem_num = semNum;
    sops.sem_op = -1;
    sops.sem_flg = 0;
    return semop(semId, &sops, 1);
}

int releaseSem(int semId, int semNum) {
    struct sembuf sops;
    sops.sem_num = semNum;
    sops.sem_op = 1;
    sops.sem_flg = 0;
    return semop(semId, &sops, 1);
}

/* int removeSem(int semId, int semNum) {
    union semun arg;
    arg.val = 0;
    return semctl(semId, semNum, IPC_RMID, arg);
} */

void initIPCs(char type) {
    /* »»»»»»»»»» SEMAFORI »»»»»»»»»» */
    if ((semId = semget(ftok("./utils/private-key", 's'), 6, IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "Semaphore pool creation failure" RESET);
        exit(EXIT_FAILURE);
    }

    /* »»»»»»»»»» MEMORIE CONDIVISE »»»»»»»»»» */
    if ((shmLedgerId = shmget(ftok("./utils/private-key", 'l'), sizeof(ledger), IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "Shared Memory creation failure Ledger" RESET);
        exit(EXIT_FAILURE);
    }

    mastro = (ledger*)shmat(shmLedgerId, NULL, 0);
    if (mastro == (void*)-1) {
        perror(RED "Shared Memory attach failure Ledger" RESET);
        exit(EXIT_FAILURE);
    }

    if ((shmUsersId = shmget(ftok("./utils/private-key", 'u'), sizeof(userProcess) * SO_USERS_NUM, IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "Shared Memory creation failure Users" RESET);
        exit(EXIT_FAILURE);
    }

    users = (userProcess*)shmat(shmUsersId, NULL, 0);
    if (users == (void*)-1) {
        perror(RED "Shared Memory attach failure Users" RESET);
        exit(EXIT_FAILURE);
    }

    if ((shmNodesId = shmget(ftok("./utils/private-key", 'n'), sizeof(nodeProcess) * SO_NODES_NUM, IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "Shared Memory creation failure Nodes" RESET);
        exit(EXIT_FAILURE);
    }

    nodes = (nodeProcess*)shmat(shmNodesId, NULL, 0);
    if (nodes == (void*)-1) {
        perror(RED "Shared Memory attach failure Nodes" RESET);
        exit(EXIT_FAILURE);
    }

    if (type == 'u' || type != 'n') {
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
    }

    if (type == 'n' || type != 'u') {
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
    }

    /* »»»»»»»»»» CODE DI MESSAGGI »»»»»»»»»» */
    if (type == 'u' || type == 'n') {
        /* in base al nodo estratto (nodeReceiver) scelgo la coda di messaggi di quel nodo */
        if ((messageQueueId = msgget(ftok("./utils/private-key", 'q'), IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
            perror(RED "Message Queue failure" RESET);
            exit(EXIT_FAILURE);
        }

        if ((responseQueueId = msgget(ftok("./utils/private-key", 'r'), IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
            perror(RED "Response Queue failure" RESET);
            exit(EXIT_FAILURE);
        }
    }
}

void removeIPCs() {
}

void error(char* txt) {
    printf("[ %sERROR%s ] %d - %s%s%s%s\n", RED, RESET, getpid(), RED, BOLD, txt, RESET);
    exit(EXIT_FAILURE);
}

void printTransaction(transaction* t) {
    if (t->sender != -1)
        printf("\t(%s%s%s%lu%s, %s%d%s → %s%d%s, %s%d%s$, %s%u%s$)\n", BOLD, YELLOW_BKG, BLUE, t->timestamp, RESET, CYAN, t->sender, RESET, CYAN, t->receiver, RESET, GREEN, t->quantity, RESET, RED, t->reward, RESET);
    else
        printf("\t(%s%s%s%lu%s, %s%s%d%s → %s%d%s, %s%d%s$, %u)\n\n", BOLD, YELLOW_BKG, BLUE, t->timestamp, RESET, BOLD, MAGENTA, t->sender, RESET, BLUE, t->receiver, RESET, RED, t->quantity, RESET, t->reward);

    /* printf("\t\t  %sTimestamp%s: %lu\n", BLUE, RESET, t->timestamp);
    printf("\t\t  %sSender%s: %d\n", BLUE, RESET, t->sender);
    printf("\t\t  %sReceiver%s: %d\n", BLUE, RESET, t->receiver);
    printf("\t\t  %sQuantity%s: %d\n", BLUE, RESET, t->quantity);
    printf("\t\t  %sReward%s: %u\n", BLUE, RESET, t->reward); */
}

void printBlock(block* b) {
    int j;
    printf("\t%s#-------------------- %sBlock%s%s --------------------%s\n", MAGENTA, BOLD, RESET, MAGENTA, RESET);
    printf("\t%sSize%s: %d\n", GREEN, RESET, b->size);
    for (j = 0; j < b->size; ++j) {
        printTransaction(&(b->transaction[j]));
    }
}

void printLedger(ledger* l) {
    int i;
    printf("\n%s%s»»»»»»»»»»»»»»»»»»»»»» Libro Mastro »»»»»»»»»»»»»»»»»»»»»»%s\n", BOLD, MAGENTA, RESET);
    printf("%sSize%s: %d\n", GREEN, RESET, l->size);
    for (i = 0; i < l->size; ++i) {
        printBlock(&(l->block[i]));
    }
}

void initVariable(char** argv) {
    SO_USERS_NUM = atoi(argv[1]);
    SO_NODES_NUM = atoi(argv[2]);

    if (strcasecmp(argv[0], "utente") == 0) { /* controllo che sia un utente */
        SO_REWARD = atoi(argv[4]);
        SO_MIN_TRANS_GEN_NSEC = atol(argv[5]);
        SO_MAX_TRANS_GEN_NSEC = atol(argv[6]);
        SO_BUDGET_INIT = atoi(argv[3]);
        SO_RETRY = atoi(argv[7]);
        offset = atoi(argv[8]);
    } else { /* altrimenti è un nodo */
        SO_TP_SIZE = atoi(argv[3]);
        SO_MIN_TRANS_PROC_NSEC = atol(argv[4]);
        SO_MAX_TRANS_PROC_NSEC = atol(argv[5]);
        offset = atoi(argv[6]);
    }
}

int alreadyVisited(transaction* lastVisited, transaction* trans) {
    int already = -1;

    if (trans->timestamp <= lastVisited->timestamp) { /* se TRUE vuol dire che ho già visitato questa transazione */
        already = 0;
    } else {
        (*lastVisited) = (*trans);
    }
    return already;
}

int balanceFromLedger(ledger* mastro, pid_t identifier, transaction* lastVisited) {
    int i, j;
    int ris = 0;
    for (i = 0; i < (mastro->size); ++i) {
        for (j = 0; j < (mastro->block[i].size); ++j) {
            if (mastro->block[i].transaction[j].receiver == identifier) {
                if (alreadyVisited(lastVisited, &(mastro->block[i].transaction[j])) == -1) {
                    ris += mastro->block[i].transaction[j].quantity;
                }
            }
        }
    }
    return ris;
}

void sleepTransaction(long min, long max) {
    long nSec = 0, sec = 0;

    nSec = (rand() % (max - min + 1)) + min;

    if (nSec > 999999999) {
        sec = nSec / 1000000000;    /* trovo la parte in secondi */
        nSec -= (sec * 1000000000); /* tolgo la parte in secondi dai nanosecondi */
    }

#ifdef DEBUG
    reserveSem(semId, print);
    printf("%sSleep for: %ld s   %ld nsec%s\n", RED, sec, nSec, RESET);
    releaseSem(semId, print);
#endif

    request.tv_nsec = nSec;
    request.tv_sec = sec;
    remaining.tv_nsec = nSec;
    remaining.tv_sec = sec;

    if (nanosleep(&request, &remaining) == -1) {
        switch (errno) {
            case EINTR: /* errno == EINTR quando nanosleep() viene interrotta da un gestore di segnali */
                break;

            default:
                perror(RED "[USER] Nanosleep fail" RESET);
                exit(EXIT_FAILURE);
        }
    }
}