#include "header.h"

#define DEBUG

/** Handler dei segnali:
 *  	-	SIGINT
 *  	-	SIGTERM
 *  	-	SIGSEGV
 *      -   SIGUSR1 (crea una nuova transazione)
 *      -   SIGUSR2 (invio transazione fallita)
 **/
void hdl(int, siginfo_t*, void*);

/** Crea una nuova transazione
 * 
 * @return la transazione da inviare al nodo
 */
transaction createTransaction();

/** Sceglie un nodo random per processare la transazione e gliela invia.
 *  In caso di fallito invio dopo SO_RETRY volte, il processo utente:
 *      -   Invia un segnale SIGUSR1 al master per notificargli la prematura morte
 *      -   Dealloca tutte le strutture IPC
 *      -   Termina
 */
void sendTransaction(transaction*);

/* Aspetta un quanto di tempo random compreso tra SO_MIN_TRANS_GEN_NSEC e SO_MAX_TRANS_GEN_NSEC */
void sleepTransactionGen();

int queueId; /* ftok(..., 'q') => 'q': queue */
int i, j, try = 0;

int main(int argc, char** argv) {
    initVariable(argv);

    /* Copia il carattere \0 in act per tutta la sua lunghezza */
    memset(&act, '\0', sizeof(act));

    /* Dico a sigaction (act) quale handler deve mandare in esecuzione */
    act.sa_sigaction = &hdl;

    act.sa_flags = SA_SIGINFO | SA_NODEFER;

    /* »»»»»»»»»» SEGNALI »»»»»»»»»» */
    if (sigaction(SIGINT, &act, NULL) < 0) {
        perror(RED "[USER] Sigaction: Failed to assign SIGINT to custom handler" RESET);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGTERM, &act, NULL) < 0) {
        perror(RED "[USER] Sigaction: Failed to assign SIGTERM to custom handler" RESET);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGSEGV, &act, NULL) < 0) {
        perror(RED "[USER] Sigaction: Failed to assign SIGSEGV to custom handler" RESET);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGUSR1, &act, NULL) < 0) {
        perror(RED "[USER] Sigaction: Failed to assign SIGUSR1 to custom handler" RESET);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGUSR2, &act, NULL) < 0) {
        perror(RED "[USER] Sigaction: Failed to assign SIGUSR2 to custom handler" RESET);
        exit(EXIT_FAILURE);
    }

    /* »»»»»»»»»» SEMAFORI »»»»»»»»»» */
    if ((semId = semget(ftok("./utils/private-key", 's'), 6, IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[USER] Semaphore pool creation failure" RESET);
        exit(EXIT_FAILURE);
    }

    /* »»»»»»»»»» MEMORIE CONDIVISE »»»»»»»»»» */
    if ((shmLedgerId = shmget(ftok("./utils/private-key", 'l'), sizeof(ledger), IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[USER] Shared Memory creation failure [LEDGER]" RESET);
        exit(EXIT_FAILURE);
    }

    mastro = (ledger*)shmat(shmLedgerId, NULL, 0);
    if (mastro == (void*)-1) {
        perror(RED "[USER] Shared Memory attach failure [LEDGER]" RESET);
        exit(EXIT_FAILURE);
    }

    if ((shmUsersId = shmget(ftok("./utils/private-key", 'u'), sizeof(userProcess) * SO_USERS_NUM, IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[USER] Shared Memory creation failure [USERS]" RESET);
        exit(EXIT_FAILURE);
    }

    users = (userProcess*)shmat(shmUsersId, NULL, 0);
    if (users == (void*)-1) {
        perror(RED "[USER] Shared Memory attach failure [USERS]" RESET);
        exit(EXIT_FAILURE);
    }

    if ((shmNodesId = shmget(ftok("./utils/private-key", 'n'), sizeof(nodeProcess) * SO_NODES_NUM, IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[USER] Shared Memory creation failure [NODES]" RESET);
        exit(EXIT_FAILURE);
    }

    nodes = (nodeProcess*)shmat(shmNodesId, NULL, 0);
    if (nodes == (void*)-1) {
        perror(RED "[USER] Shared Memory attach failure [NODES]" RESET);
        exit(EXIT_FAILURE);
    }

    /* per sincronizzare i processi alla creazione iniziale, e dare loro il via con le operazioni */
    shmActiveUsersId = shmget(ftok("./utils/key-private", 'a'), sizeof(int), IPC_CREAT | 0644);
    if (shmActiveUsersId < 0) {
        perror(RED "[USER] Shared Memory creation failure [ActiveProcess]" RESET);
        exit(EXIT_FAILURE);
    }

    activeUsers = (int*)shmat(shmActiveUsersId, NULL, 0);
    if (activeUsers == (void*)-1) {
        perror(RED "[USER] Shared Memory attach failure [ActiveProcess]" RESET);
        exit(EXIT_FAILURE);
    }

    /* »»»»»»»»»» CODA DI MESSAGGI »»»»»»»»»» */
    /* in base al nodo estratto (nodeReceiver) scelgo la coda di messaggi di quel nodo */
    if ((queueId = msgget(ftok("./utils/private-key", 'q'), IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[USER] Message Queue failure" RESET);
        exit(EXIT_FAILURE);
    }

    /* Il semaforo 2 viene utilizzato per accedere in mutua esclusione all'area di shared memory per sincronizzarsi */
    reserveSem(semId, userSync);
    (*activeUsers)++;
    releaseSem(semId, userSync);

#ifdef DEBUG
    reserveSem(semId, print);
    printf("[%s%d%s] USER - Wrote +1 in shared\n", CYAN, getpid(), RESET);
    releaseSem(semId, print);
#endif

    srand(time(NULL) % getpid()); /* randomize seed */

    reserveSem(semId, userShm);
    (users + offset)->pid = getpid();           /* salvo il PID del processo in esecuzione nella memoria condivisa con la lista degli utenti */
    (users + offset)->balance = SO_BUDGET_INIT; /* inizializzo il bilancio a SO_BUDGET_INIT */
    balance = &((users + offset)->balance);     /* RIFERIMENTO al bilancio */
    releaseSem(semId, userShm);

#ifdef DEBUG
    reserveSem(semId, print);
    printf("[ %s%d%s ] BILANCIO INIZIALE: %d\n", CYAN, getpid(), RESET, *balance);
    releaseSem(semId, print);
#endif

    while (1) {
        reserveSem(semId, userShm); /* aggiorno il bilancio */
        (*balance) += balanceFromLedger(mastro, getpid(), &lastVisited);
        releaseSem(semId, userShm);

#ifdef DEBUG
        reserveSem(semId, print);
        printf("[ %s%d%s ] BILANCIO: %d\n", CYAN, getpid(), RESET, *balance);
        releaseSem(semId, print);
#endif

        if ((*balance) >= 2) {
            transaction trans;

            trans = createTransaction();

            sendTransaction(&trans);

#ifdef DEBUG
            reserveSem(semId, print);
            printf("[ %s%d%s ] BILANCIO PRIMA: %d\n", CYAN, getpid(), RESET, *balance);
            releaseSem(semId, print);
#endif

            reserveSem(semId, print);
            printTransaction(&trans);
            releaseSem(semId, print);

            reserveSem(semId, userShm);
            (*balance) -= (trans.quantity + trans.reward);
            releaseSem(semId, userShm);

#ifdef DEBUG
            reserveSem(semId, print);
            printf("[ %s%d%s ] BILANCIO DOPO: %d\n", CYAN, getpid(), RESET, *balance);
            releaseSem(semId, print);
#endif

            sleep(2);
            /* sleepTransactionGen(); */
        } else {
            /* TEST */
            /* printf("NOTIFICO IL MASTER CHE NON HO PIU' SOLDI\n"); */
            kill(getppid(), SIGUSR1);
            exit(EXIT_FAILURE);
        }
    }

    printf("\t%d Termino...\n", getpid());

    exit(EXIT_SUCCESS);
}

void hdl(int sig, siginfo_t* siginfo, void* context) {
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

            try++;

            if (try == SO_RETRY) { /* non è riuscito ad inviare la transazione */
                reserveSem(semId, print);
                printf("[ %s%d%s ] Failed to send transaction for SO_RETRY times\n", CYAN, getpid(), RESET);
                releaseSem(semId, print);

                shmdt(mastro);
                shmdt(users);
                shmdt(nodes);
                shmdt(activeUsers);

                exit(EXIT_FAILURE);
            }
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