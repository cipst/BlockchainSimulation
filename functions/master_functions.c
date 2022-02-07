#include "../headers/master.h"

void hdl(int sig, siginfo_t* siginfo, void* context) {
    /**
	 * 	Questo è l'handler dei segnali, gestisce i segnali:
	 * 		-	SIGALRM
	 *  	-	SIGINT
	 *  	-	SIGTERM
	 *  	-	SIGSEGV
     *      -   SIGUSR1
     *      -   SIGUSR2
	 **/

    switch (sig) {
        pid_t term;

        case SIGALRM:
            /* if (sigaction(SIGALRM, &act, NULL) < 0) {
                perror(RED "Sigaction: Failed to assign SIGALRM to custom handler" RESET);
                exit(EXIT_FAILURE);
            } */

            printf("\n%sSO_SIM_SEC%s expired - Termination of the simulation...\n", YELLOW, RESET);

            killAll(SIGINT);

            while ((term = wait(NULL)) != -1) {
                /* #ifdef DEBUG */
                reserveSem(semId, print);
                fflush(stdout);
                printf("%d %s%sENDED%s!\n", term, BOLD, RED, RESET);
                releaseSem(semId, print);
                /* #endif */
            }

            printStats();

            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            shmdt(activeUsers);
            shmdt(activeNodes);

            system("./ipcrm.sh");
            exit(EXIT_SUCCESS);

        case SIGINT:
#ifdef DEBUG
            printf("\n[ %sSIGINT%s ] %d sended SIGINT\n", YELLOW, RESET, siginfo->si_pid);
#endif

            signal(SIGINT, SIG_IGN);
            kill(0, SIGINT);
            sigaction(SIGINT, &act, NULL);

            printStats();

            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            shmdt(activeUsers);
            shmdt(activeNodes);

            system("./ipcrm.sh");
            printf("[ %sSIGINT%s ] Simulation interrupted\n", YELLOW, RESET);
            exit(EXIT_SUCCESS);

        case SIGTERM:
            kill(0, SIGINT);
            break;

        case SIGSEGV:
            printf("\n[ %sSIGSEGV%s ] Simulation interrupted due to SIGSEGV\n", RED, RESET);
            killAll(SIGINT);
            exit(EXIT_FAILURE);

        case SIGUSR1: /* i processi utente utilizzano SIGUSR1 per notificare al master che sono terminati prematuramente */

            reserveSem(semId, print);
            printf("[ %sSIGUSR1%s ] User %s%d%s terminated prematurely\n", YELLOW, RESET, CYAN, siginfo->si_pid, RESET);
            releaseSem(semId, print);

            reserveSem(semId, userSync);
            (*activeUsers)--;

            reserveSem(semId, print);
            printf("[ %sSIGUSR1%s ] Active users: %d\n", YELLOW, RESET, *activeUsers);
            releaseSem(semId, print);

            releaseSem(semId, userSync);

            if ((*activeUsers) == 0) { /* tutti gli utenti hanno terminato la loro esecuzione */
                printf("\n[ %s%smaster%s ] All users ended. %sEnd of the simulation...%s", BOLD, GREEN, RESET, YELLOW, RESET);
                printStats();
                killAll(SIGINT);
                shmdt(mastro);
                shmdt(users);
                shmdt(nodes);
                shmdt(activeUsers);
                shmdt(activeNodes);
                system("./ipcrm.sh");

                exit(EXIT_FAILURE);
            }
            break;

        case SIGUSR2:
            reserveSem(semId, print);
            printf("[ %sSIGUSR2%s ] Node %s%d%s found %s%sLIBRO MASTRO%s full!\n", YELLOW, RESET, BLUE, siginfo->si_pid, RESET, BOLD, GREEN, RESET);
            releaseSem(semId, print);

            killAll(SIGINT);

            while ((term = wait(NULL)) != -1) {
                /* #ifdef DEBUG */
                reserveSem(semId, print);
                fflush(stdout);
                printf("%d %s%sENDED%s!\n", term, BOLD, RED, RESET);
                releaseSem(semId, print);
                /* #endif */
            }

            printStats();

            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            shmdt(activeUsers);
            shmdt(activeNodes);
            system("./ipcrm.sh");

            exit(EXIT_FAILURE);
    }
}

void createNode(int pos) {
    char* arg[7]; /* salvo le informazioni da passare al processo nodo */
    /* char usersNum[12];
    char nodesNum[12]; */
    char tpSize[12];
    char minTransProc[12];
    char maxTransProc[12];
    char offset[12];
    char hops[12];

    /* sprintf(usersNum, "%d", SO_USERS_NUM);
    sprintf(nodesNum, "%d", SO_NODES_NUM); */
    sprintf(tpSize, "%d", SO_TP_SIZE);
    sprintf(minTransProc, "%ld", SO_MIN_TRANS_PROC_NSEC);
    sprintf(maxTransProc, "%ld", SO_MAX_TRANS_PROC_NSEC);
    sprintf(offset, "%d", pos);
    sprintf(hops, "%d", SO_HOPS);

    arg[0] = "nodo";
    /* arg[1] = usersNum;
    arg[2] = nodesNum; */
    arg[1] = tpSize;
    arg[2] = minTransProc;
    arg[3] = maxTransProc;
    arg[4] = offset; /* offset per la memoria condivisa 'nodes', ogni nodo è a conoscenza della sua posizione in 'nodes' */
    arg[5] = hops;
    arg[6] = NULL;

    if (execv("./nodo.o", arg) < 0) {
        perror(RED "Failed to launch execv [NODO]" RESET);
        exit(EXIT_FAILURE);
    }
}

void createUser(int pos) {
    char* arg[9]; /* salvo le informazioni da passare al processo nodo */
    char usersNum[12];
    char budgetInit[12];
    char reward[12];
    char minTransGen[12];
    char maxTransGen[12];
    char retry[12];
    char offset[12];

    sprintf(usersNum, "%d", SO_USERS_NUM);
    sprintf(budgetInit, "%d", SO_BUDGET_INIT);
    sprintf(reward, "%d", SO_REWARD);
    sprintf(minTransGen, "%ld", SO_MIN_TRANS_GEN_NSEC);
    sprintf(maxTransGen, "%ld", SO_MAX_TRANS_GEN_NSEC);
    sprintf(retry, "%d", SO_RETRY);
    sprintf(offset, "%d", pos);

    arg[0] = "utente";
    arg[1] = usersNum;
    arg[2] = budgetInit;
    arg[3] = reward;
    arg[4] = minTransGen;
    arg[5] = maxTransGen;
    arg[6] = retry;
    arg[7] = offset; /* offset per la memoria condivisa 'users', ogni utente è a conoscenza della sua posizione in 'users' */
    arg[8] = NULL;

    if (execv("./utente.o", arg) < 0) {
        perror(RED "Failed to launch execv [UTENTE]" RESET);
        exit(EXIT_FAILURE);
    }
}

void killAll(int sig) {
    int i;

    for (i = 0; i < SO_USERS_NUM; i++) {
        if ((users + i)->pid != 0) {
            kill((users + i)->pid, sig);
        }
    }

    for (i = 0; i < SO_NODES_NUM; i++) {
        if ((nodes + i)->pid != 0) {
            kill((nodes + i)->pid, sig);
        }
    }
}

void printIpcStatus() {
    const char* aux = GREEN "IPC" RESET;
    printf("\n[ %s ] Printing ipc objects status\n", aux);
    printf("\t[ %s ] semid: %d\n", aux, semId);
    printf("\t[ %s ] shmLedgerId: %d\n", aux, shmLedgerId);
    printf("\t[ %s ] shmUsersId: %d\n", aux, shmUsersId);
    printf("\t[ %s ] shmNodesId: %d\n", aux, shmNodesId);
    printf("\t[ %s ] shmActiveUsersId: %d\n", aux, shmActiveUsersId);
    printf("\t[ %s ] shmActiveNodesId: %d\n", aux, shmActiveNodesId);
}


void initMasterIPC() {
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

    /* »»»»»»»»»» CODE DI MESSAGGI »»»»»»»»»» */
    if ((friendsQueueId = msgget(ftok("./utils/private-key", 'f'), IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "Friends Queue failure" RESET);
        exit(EXIT_FAILURE);
    }
}


void tooManyProcess(char type) {
    int i, max, min, count = 0;
    if (type == 'u') {
        max = min = users->balance; /* ipotizzo che il primo sia il max */

        /* cerco il massimo e il minimo*/
        for (i = 1; i < SO_USERS_NUM; ++i) {
            if ((users + i)->balance > max) {
                max = (users + i)->balance;
            }
            if ((users + i)->balance < min) {
                min = (users + i)->balance;
            }
        }

        /* stampo a video TOO_MANY_USERS/2 valori massimi */
        printf("[ %s%smaster%s ] Balance of the most significant users:\n", BOLD, GREEN, RESET);
        for (i = 0; count < (TOO_MANY_USERS / 2) && i < SO_USERS_NUM; ++i) {
            if ((users + i)->balance = max) {
                printf("\t [ %s%d%s ] Balance: %d\n", CYAN, (users + i)->pid, RESET, (users + i)->balance);
                count++;
            }
        }

        count = 0;
        /* stampo a video TOO_MANY_USERS/2 valori minimi */
        for (i = 0; count < (TOO_MANY_USERS / 2) && i < SO_USERS_NUM; ++i) {
            if ((users + i)->balance == min) {
                printf("\t [ %s%d%s ] Balance: %d\n", CYAN, (users + i)->pid, RESET, (users + i)->balance);
                count++;
            }
        }
    }

    if (type == 'n') {
        max = min = nodes->balance; /* ipotizzo che il primo sia il max */

        /* cerco il massimo e il minimo*/
        for (i = 1; i < SO_NODES_NUM; ++i) {
            if ((nodes + i)->balance > max) {
                max = (nodes + i)->balance;
            }
            if ((nodes + i)->balance < min) {
                min = (nodes + i)->balance;
            }
        }

        /* stampo a video i primi TOO_MANY_NODES valori massimi e minimi */
        printf("[ %s%smaster%s ] Balance of the most significant nodes:\n", BOLD, GREEN, RESET);
        for (i = 0; count < (TOO_MANY_NODES / 2) && i < SO_NODES_NUM; ++i) {
            if ((nodes + i)->balance == max) {
                printf("\t [ %s%d%s ] Balance: %d\n", CYAN, (nodes + i)->pid, RESET, (nodes + i)->balance);
                count++;
            }
        }

        count = 0;
        /* stampo a video TOO_MANY_NODES/2 valori minimi */
        for (i = 0; count < (TOO_MANY_NODES / 2) && i < SO_NODES_NUM; ++i) {
            if ((nodes + i)->balance == min) {
                printf("\t [ %s%d%s ] Balance: %d\n", CYAN, (nodes + i)->pid, RESET, (nodes + i)->balance);
                count++;
            }
        }
    }
}

void setAllFriends() {
    int i, j;

    for (i = 0; i < SO_NODES_NUM; ++i) {
        (nodes + i)->friends = (pid_t*)malloc(SO_NUM_FRIENDS * sizeof(pid_t)); /* alloco lo spazio in memoria necessario per la memorizzazione di SO_NUM_FRIENDS pid di nodi amici */
        (nodes + i)->friendNum = SO_NUM_FRIENDS;                               /* salvo il numero di amici che il nodo 'i' ha, così da poter gestire il caso in cui si aggiunga un amico */

        /* gli amici di un nodo 'i' sono i successivi SO_NUM_FRIENDS nodi */
        /* rimango dentro al ciclo finchè j non raggiunge SO_NUM_FREINDS */
        for (j = 0; j < SO_NUM_FRIENDS; ++j) {
            (nodes + i)->friends[j] = (nodes + ((j + i + 1) % SO_NODES_NUM))->pid;
        }
    }

    reserveSem(semId, print);
    printf("[ %s%smaster%s ] All nodes friends setted!\n", BOLD, GREEN, RESET);
    releaseSem(semId, print);
}

void printStats() {
    int k;
    const char aux[] = MAGENTA "STATS" RESET;

    reserveSem(semId, print);
    printf("\n\n[ %s ] Stats of the simulation\n", aux);

    printf("\t[ %s ] Total %susers%s in the simulation: %d\n", aux, CYAN, RESET, SO_USERS_NUM);

    printf("\t[ %s ] Total %snodes%s in the simulation: %d\n\n", aux, BLUE, RESET, SO_NODES_NUM);

    printf("\t[ %s ] Balance of every users:\n", aux);
    for (k = 0; k < SO_USERS_NUM; ++k)
        printf("\t\t [ %s%d%s ] Balance: %d\n", CYAN, (users + k)->pid, RESET, (users + k)->balance);

    printf("\n\t[ %s ] Balance of every nodes and number of transactions in TP:\n", aux);
    for (k = 0; k < SO_NODES_NUM; ++k)
        printf("\t\t [ %s%d%s ] Balance: %d | Transactions remaining: %d/%d\n", BLUE, (nodes + k)->pid, RESET, (nodes + k)->balance, (nodes + k)->poolSize, SO_TP_SIZE);

    reserveSem(semId, userSync);
    printf("\n\t[ %s ] Process terminated prematurely: %d/%d\n", aux, (SO_USERS_NUM - (*activeUsers)), SO_USERS_NUM);
    releaseSem(semId, userSync);

    printf("\n\t[ %s ] Blocks in the Ledger: %d/%d\n", aux, mastro->size, SO_REGISTRY_SIZE);

    printLedger(mastro);

    printf("\n");

    releaseSem(semId, print);
}

void readConfigFile() {
    FILE* fp;                                        /* puntatore a file */
    if ((fp = fopen("./utils/config", "r")) == NULL) /* apertura del file di configurazione in sola lettura */
        error("File Not Found!");

    fscanf(fp, "SO_USERS_NUM: %d\n", &SO_USERS_NUM);                             /* numero di processi utente che possono inviare denaro ad altri utenti attraverso una transazione */
    fscanf(fp, "SO_NODES_NUM: %d\n", &SO_NODES_NUM);                             /* numero di processi nodo che elaborano, a pagamento, le transazioni ricevute */
    fscanf(fp, "SO_BUDGET_INIT: %d\n", &SO_BUDGET_INIT);                         /* budget iniziale di ciascun processo utente */
    fscanf(fp, "SO_REWARD: %d\n", &SO_REWARD);                                   /* la percentuale di reward pagata da ogni utente per il processamento di una transazione */
    fscanf(fp, "SO_MIN_TRANS_GEN_NSEC [nsec]: %ld\n", &SO_MIN_TRANS_GEN_NSEC);   /* minimo valore del tempo (espresso in nanosecondi) che trascorre fra la generazione di una transazione e la seguente da parte di un utente */
    fscanf(fp, "SO_MAX_TRANS_GEN_NSEC [nsec]: %ld\n", &SO_MAX_TRANS_GEN_NSEC);   /* massimo valore del tempo (espresso in nanosecondi) che trascorre fra la generazione di una transazione e la seguente da parte di un utente */
    fscanf(fp, "SO_RETRY: %d\n", &SO_RETRY);                                     /* numero di tentativi che un processo utente ha per inviare una transazione */
    fscanf(fp, "SO_TP_SIZE: %d\n", &SO_TP_SIZE);                                 /* numero massimo di transazioni nella transaction pool dei processi nodo */
    fscanf(fp, "SO_MIN_TRANS_PROC_NSEC [nsec]: %ld\n", &SO_MIN_TRANS_PROC_NSEC); /* minimo valore del tempo simulato (espresso in nanosecondi) di processamento di un blocco da parte di un nodo */
    fscanf(fp, "SO_MAX_TRANS_PROC_NSEC [nsec]: %ld\n", &SO_MAX_TRANS_PROC_NSEC); /* massimo valore del tempo simulato (espresso in nanosecondi) di processamento di un blocco da parte di un nodo */
    fscanf(fp, "SO_SIM_SEC: %ld\n", &SO_SIM_SEC);                                /* durata della simulazione (in secondi) */
    fscanf(fp, "SO_NUM_FRIENDS: %d\n", &SO_NUM_FRIENDS);                         /* IMPORTANT numero di nodi amici dei processi nodo (solo per la versione full) */
    fscanf(fp, "SO_HOPS: %d\n", &SO_HOPS);                                       /* IMPORTANT numero massimo di salti massimo che una transazione può effettuare quando la transaction pool di un nodo è piena (solo per la versione full) */

    fclose(fp); /* chiusura del file */

    if (SO_USERS_NUM <= 0)
        error("SO_USERS_NUM must be greater than 0.");

    if (SO_NODES_NUM <= 0)
        error("SO_NODES_NUM must be greater than 0.");

    if (SO_REWARD <= 0 || SO_REWARD >= 100)
        error("SO_REWARD must be greater than 0, but less than 100.");

    if (SO_MIN_TRANS_GEN_NSEC < 0)
        error("SO_MIN_TRANS_GEN_NSEC must be greater than or equal to 0.");

    if (SO_MAX_TRANS_GEN_NSEC <= SO_MIN_TRANS_GEN_NSEC)
        error("SO_MAX_TRANS_GEN_NSEC must be greater than SO_MIN_TRANS_GEN_NSEC.");

    if (SO_RETRY <= 0)
        error("SO_RETRY must be greater than 0.");

    if (SO_TP_SIZE <= 0 || SO_TP_SIZE <= SO_BLOCK_SIZE)
        error("SO_TP_SIZE must be greater than 0 and greater than SO_BLOCK_SIZE.");

    if (SO_MIN_TRANS_PROC_NSEC < 0)
        error("SO_MIN_TRANS_PROC_NSEC must be greater than or equal to 0.");

    if (SO_MAX_TRANS_PROC_NSEC <= SO_MIN_TRANS_PROC_NSEC)
        error("SO_MAX_TRANS_PROC_NSEC must be greater than SO_MIN_TRANS_PROC_NSEC.");

    if (SO_BUDGET_INIT <= 1)
        error("SO_BUDGET_INIT must be greater than 1.");

    if (SO_SIM_SEC <= 0)
        error("SO_SIM_SEC must be greater than 0.");

    if (SO_NUM_FRIENDS < 0 || SO_NUM_FRIENDS >= SO_NODES_NUM)
        error("SO_NUM_FRIENDS must be greater than or equal to 0 but less than SO_NODES_NUM.");

    if (SO_HOPS < 0)
        error("SO_HOPS must be greater than or equal to 0.");
}

void printConfigVal() {
    printf("\nSO_USERS_NUM: %d\n", SO_USERS_NUM);
    printf("SO_NODES_NUM: %d\n", SO_NODES_NUM);
    printf("SO_REWARD: %d\n", SO_REWARD);
    printf("SO_MIN_TRANS_GEN_NSEC [nsec]: %ld\n", SO_MIN_TRANS_GEN_NSEC);
    printf("SO_MAX_TRANS_GEN_NSEC [nsec]: %ld\n", SO_MAX_TRANS_GEN_NSEC);
    printf("SO_RETRY: %d\n", SO_RETRY);
    printf("SO_TP_SIZE: %d\n", SO_TP_SIZE);
    printf("SO_MIN_TRANS_PROC_NSEC [nsec]: %ld\n", SO_MIN_TRANS_PROC_NSEC);
    printf("SO_MAX_TRANS_PROC_NSEC [nsec]: %ld\n", SO_MAX_TRANS_PROC_NSEC);
    printf("SO_BUDGET_INIT: %d\n", SO_BUDGET_INIT);
    printf("SO_SIM_SEC: %ld\n", SO_SIM_SEC);
    printf("SO_NUM_FRIENDS: %d\n", SO_NUM_FRIENDS);
    printf("SO_HOPS: %d\n", SO_HOPS);
    printf(" #SO_BLOCK_SIZE: %d\n", SO_BLOCK_SIZE);
    printf(" #SO_REGISTRY_SIZE: %d\n\n", SO_REGISTRY_SIZE);
}