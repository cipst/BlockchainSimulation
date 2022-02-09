#include "./headers/master.h"

/* #define DEBUG */

int i, j, stop = 0;

int main(int argc, char** argv) {
    if (argc != 1) /* controllo sul numero di argomenti */
        error("Usage: ./master.o");

    system("./ipcrm.sh"); /* rimuovo eventuali risorse IPC rimaste da un'esecuzione non terminata correttamente */

    printf("[ %s%smaster - %d%s ] Init IPC objects and all necessary resources. \n", BOLD, GREEN, getpid(), RESET);

    readConfigFile(); /* lettura del file di configurazione */

#ifdef DEBUG
    printConfigVal();
#endif

    /* Copia il carattere \0 in act per tutta la sua lunghezza */
    memset(&act, '\0', sizeof(act));

    /* Dico a sigaction (act) quale handler deve mandare in esecuzione */
    act.sa_sigaction = &hdl;

    act.sa_flags = SA_SIGINFO | SA_NODEFER;

    /* »»»»»»»»»» SEGNALI »»»»»»»»»» */
    if (sigaction(SIGINT, &act, NULL) < 0) {
        perror(RED "Sigaction: Failed to assign SIGINT to custom handler" RESET);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGTERM, &act, NULL) < 0) {
        perror(RED "Sigaction: Failed to assign SIGTERM to custom handler" RESET);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGSEGV, &act, NULL) < 0) {
        perror(RED "Sigaction: Failed to assign SIGSEGV to custom handler" RESET);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGALRM, &act, NULL) < 0) {
        perror(RED "Sigaction: Failed to assign SIGALRM to custom handler" RESET);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGUSR1, &act, NULL) < 0) {
        perror(RED "Sigaction: Failed to assign SIGUSR1 to custom handler" RESET);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGUSR2, &act, NULL) < 0) {
        perror(RED "Sigaction: Failed to assign SIGUSR2 to custom handler" RESET);
        exit(EXIT_FAILURE);
    }

    initMasterIPC(); /* inizializzo le risorse IPC */

    if (initSemAvailable(semId, userSync) < 0) { /* semaforo per l'accesso alla memoria condivisa di sincronizzazione degli Utenti */
        perror(RED "Failed to initialize semaphore 0" RESET);
        exit(EXIT_FAILURE);
    }

    if (initSemAvailable(semId, nodeSync) < 0) { /* semaforo per l'accesso alla memoria condivisa di sincronizzazione dei nodi */
        perror(RED "Failed to initialize semaphore 1" RESET);
        exit(EXIT_FAILURE);
    }

    if (initSemAvailable(semId, userShm) < 0) { /* semaforo per l'accesso in mutua esclusione alla memoria condivisa USERS */
        perror(RED "Failed to initialize semaphore 2" RESET);
        exit(EXIT_FAILURE);
    }

    if (initSemAvailable(semId, nodeShm) < 0) { /* semaforo per l'accesso in mutua esclusione alla memoria condivisa NODES */
        perror(RED "Failed to initialize semaphore 3" RESET);
        exit(EXIT_FAILURE);
    }

    if (initSemAvailable(semId, ledgerShm) < 0) { /* semaforo per l'accesso in mutua esclusione alla memoria condivisa LEDGER (Libro Mastro) */
        perror(RED "Failed to initialize semaphore 4" RESET);
        exit(EXIT_FAILURE);
    }
    if (initSemAvailable(semId, print) < 0) { /* semaforo per non sovrapporre la stampa a video */
        perror(RED "Failed to initialize semaphore 5" RESET);
        exit(EXIT_FAILURE);
    }

    /* Inizializzo activeUsers a 0 => nessun processo 'utente' è attivo */
    reserveSem(semId, userSync);
    *activeUsers = 0;
    releaseSem(semId, userSync);

    /* Inizializzo activeNodes a 0 => nessun processo 'nodo' è attivo */
    reserveSem(semId, nodeSync);
    *activeNodes = 0;
    releaseSem(semId, nodeSync);

    printIpcStatus(); /* stato degli oggetti IPC */

    sleep(1);

    reserveSem(semId, userShm);
    reserveSem(semId, nodeShm);

    for (i = 0; i < SO_NODES_NUM; ++i) { /* genero SO_NODES_NUM processi nodo */
        switch (fork()) {
            case -1:
                perror(RED "Fork: Failed to create a child process" RESET);
                exit(EXIT_FAILURE);
                break;

            case 0: {
                createNode(i);
                break;
            }

            default:
                break;
        }
    }

    printf("[ %s%smaster%s ] All NODE processes generated. \n", BOLD, GREEN, RESET);

    for (i = 0; i < SO_USERS_NUM; ++i) { /* genero SO_USERS_NUM processi utente */
        switch (fork()) {
            case -1:
                perror(RED "Fork: Failed to create a child process" RESET);
                exit(EXIT_FAILURE);
                break;

            case 0: {
                createUser(i);
                break;
            }

            default:
                break;
        }
    }

    printf("[ %s%smaster%s ] All processes generated. \n", BOLD, GREEN, RESET);

#ifdef DEBUG
    reserveSem(semId, print);
    reserveSem(semId, userSync);
    printf("[ %s%smaster%s ] Active user: %d. \n", BOLD, GREEN, RESET, *activeUsers);
    releaseSem(semId, userSync);

    reserveSem(semId, nodeSync);
    printf("[ %s%smaster%s ] Active node: %d. \n", BOLD, GREEN, RESET, *activeNodes);
    releaseSem(semId, nodeSync);
    releaseSem(semId, print);
#endif

    while (stop == 0) { /* aspetto finchè non ho SO_USERS_NUM processi utente attivi e SO_NODES_NUM processi nodo attivi */

        reserveSem(semId, userSync);
        reserveSem(semId, nodeSync);
        if ((*activeUsers) == SO_USERS_NUM && (*activeNodes) == SO_NODES_NUM)
            stop++;
        releaseSem(semId, userSync);
        releaseSem(semId, nodeSync);

        /* sleep(1); */
    }

#ifdef DEBUG
    reserveSem(semId, print);
    reserveSem(semId, userSync);
    printf("[ %s%smaster%s ] Active user: %d. \n", BOLD, GREEN, RESET, *activeUsers);
    releaseSem(semId, userSync);

    reserveSem(semId, nodeSync);
    printf("[ %s%smaster%s ] Active node: %d. \n", BOLD, GREEN, RESET, *activeNodes);
    releaseSem(semId, nodeSync);
    releaseSem(semId, print);
#endif

    releaseSem(semId, nodeShm);

    reserveSem(semId, nodeShm);
    /* una volta creati tutti i nodi posso assegnare a loro dei nodi amici */
    setAllFriends();
    releaseSem(semId, nodeShm);

    releaseSem(semId, userShm);

    printf("[ %s%smaster%s ] All processes are starting...\n", BOLD, GREEN, RESET);

    srand(time(NULL) % getpid()); /* randomize seed */

    alarm(SO_SIM_SEC);

#ifdef DEBUG
    reserveSem(semId, nodeSync);
    printf("[ %s%smaster%s ] Simulation Time setted to %ld seconds \n", BOLD, GREEN, RESET, SO_SIM_SEC);
    releaseSem(semId, nodeSync);
#endif

    while (1) {
        int k, z;
        message msg;

        sleep(1);

        reserveSem(semId, print);

        reserveSem(semId, userSync);
        printf("\n[ %s%smaster%s ] User active: %d\n", BOLD, GREEN, RESET, *activeUsers);
        releaseSem(semId, userSync);

        reserveSem(semId, nodeSync);
        printf("[ %s%smaster%s ] Node active: %d\n", BOLD, GREEN, RESET, *activeNodes);
        releaseSem(semId, nodeSync);

        reserveSem(semId, userShm);
        if (SO_USERS_NUM > TOO_MANY_USERS) {
            tooManyProcess('u');
        } else {
            printf("[ %s%smaster%s ] Balance of every users:\n", BOLD, GREEN, RESET);
            for (k = 0; k < SO_USERS_NUM; ++k)
                printf("\t [ %s%d%s ] Balance: %d\n", CYAN, (users + k)->pid, RESET, (users + k)->balance);
        }
        releaseSem(semId, userShm);

        reserveSem(semId, nodeShm);
        if (SO_NODES_NUM > TOO_MANY_NODES) {
            tooManyProcess('n');
        } else {
            printf("[ %s%smaster%s ] Balance of every nodes:\n", BOLD, GREEN, RESET);
            for (k = 0; k < SO_NODES_NUM; ++k)
                printf("\t [ %s%d%s ] Balance: %d\n", BLUE, (nodes + k)->pid, RESET, (nodes + k)->balance);
        }
        releaseSem(semId, nodeShm);

        printf("\n");

        releaseSem(semId, print);

        if ((rand() % 5) == 0) {
            int userOffset = (rand() % SO_USERS_NUM);
            kill((users + userOffset)->pid, SIGUSR1);
        }

        if (msgrcv(friendsQueueId, &msg, sizeof(msg) - sizeof(long), getpid(), IPC_NOWAIT | MSG_NOERROR) < 0) {
            if (errno != ENOMSG) {
                perror(RED "[USER] Error in responseQueueId msgrcv()" RESET);
                exit(EXIT_FAILURE);
            }
        } else {
            int newPos;

            newPos = (SO_NODES_NUM++);

            /* creo il nuovo nodo */
            switch (fork()) {
                case -1:
                    perror(RED "Fork: Failed to create a child process" RESET);
                    exit(EXIT_FAILURE);
                    break;

                case 0: {
                    createNode(newPos);
                    break;
                }

                default:
                    break;
            }

            /* imposto gli amici per questo nodo */
            setFriends(newPos);

            
        }
    }
}
