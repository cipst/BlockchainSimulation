#include "../header.h"

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

void error(char* txt) {
    printf("[ %sERROR%s ] %d - %s%s%s%s\n", RED, RESET, getpid(), RED, BOLD, txt, RESET);
    exit(EXIT_FAILURE);
}

void printTransaction(transaction* t) {
    printf("\t\t%s#----Transaction----%s\n", YELLOW, RESET);
    printf("\t\t  %sTimestamp%s: %lu\n", BLUE, RESET, t->timestamp);
    printf("\t\t  %sSender%s: %d\n", BLUE, RESET, t->sender);
    printf("\t\t  %sReceiver%s: %d\n", BLUE, RESET, t->receiver);
    printf("\t\t  %sQuantity%s: %d\n", BLUE, RESET, t->quantity);
    printf("\t\t  %sReward%s: %u\n", BLUE, RESET, t->reward);
}

void printBlock(block* b) {
    int j;
    printf("\t%s#------Block------%s\n", CYAN, RESET);
    printf("\t  %sSize%s: %d\n", GREEN, RESET, b->size);
    for (j = 0; j < b->size; ++j) {
        printTransaction(&(b->transaction[j]));
    }
}

void printLedger(ledger* l) {
    int i;
    printf("\n%s»»»»»»»» Libro Mastro »»»»»»»»%s\n", MAGENTA, RESET);
    printf("  %sSize%s: %d\n", GREEN, RESET, l->size);
    for (i = 0; i < l->size; ++i) {
        printBlock(&(l->block[i]));
    }
}

void initVariable(char** argv) {
    SO_USERS_NUM = atoi(argv[1]);
    SO_NODES_NUM = atoi(argv[2]);
    SO_BUDGET_INIT = atoi(argv[3]);
    SO_MIN_TRANS_GEN_NSEC = atol(argv[5]);
    SO_MAX_TRANS_GEN_NSEC = atol(argv[6]);
    SO_RETRY = atoi(argv[7]);
    offset = atoi(argv[8]);

    if (strcasecmp(argv[0], "./utente")) { /* controllo che sia un utente */
        SO_REWARD = atoi(argv[4]);
    } else { /* altrimenti è un nodo */
        SO_TP_SIZE = atoi(argv[4]);
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
    for (i = 0; i < mastro->size; ++i) {
        for (j = 0; j < mastro->block[i].size; ++j) {
            if (mastro->block[i].transaction[j].receiver == identifier) {
                if (alreadyVisited(lastVisited, &(mastro->block[i].transaction[j])) == -1) {
                    ris += mastro->block[i].transaction[j].quantity;
                }
            }
        }
    }
    return ris;
}