#include "master.h"

/* #define DEBUG */

/* Gestore dei segnali */
void hdl(int, siginfo_t*, void*);

/** Calcola il bilancio delle transazioni presenti nel Libro Mastro 
 * 
 * @return il bilancio (entrate/uscite) delle transazioni fatte dall'utente
 */
int balanceFromLedger();

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
void sendTransaction(transaction);

/* Aspetta un quanto di tempo random compreso tra SO_MIN_TRANS_GEN_NSEC e SO_MAX_TRANS_GEN_NSEC */
void sleepTransactionGen();

struct sigaction act;
struct timespec tp;
struct timespec remaining, request;

int semId;              /* ftok(..., 's') => 's': semaphore */
int queueId;            /* ftok(..., 'q') => 'q': queue */
int shmLedgerId;        /* ftok(..., 'l') => 'l': ledger */
int shmUsersId;         /* ftok(..., 'u') => 'u': users */
int shmNodesId;         /* ftok(..., 'n') => 'n': nodes */
int shmActiveProcessId; /* ftok(..., 'p') => 'p': process */

ledger* mastro;     /* informazioni sul libro mastro in memoria condivisa */
process* users;     /* informazioni sugli utenti in memoria condivisa */
process* nodes;     /* informazione sui nodi in memoria condivisa */
int* activeProcess; /* conteggio dei processi attivi in shared memory */

int SO_USERS_NUM;           /* numero di utenti nella simulazione */
int SO_NODES_NUM;           /* numero di nodi nella simulazione */
int SO_BUDGET_INIT;         /* budget iniziale di un utente */
int SO_REWARD;              /* SO_REWARD da applicare alla transazione */
long SO_MIN_TRANS_GEN_NSEC; /* minimo valore del tempo (espresso in nanosecondi) che trascorre fra la generazione di una transazione e la seguente da parte di un utente */
long SO_MAX_TRANS_GEN_NSEC; /* massimo valore del tempo (espresso in nanosecondi) che trascorre fra la generazione di una transazione e la seguente da parte di un utente */
int SO_RETRY;               /* numero massimo di fallimenti consecutivi nella generazione di transazioni dopo cui un processo utente termina */
int offset;                 /* offset per la memoria condivisa 'users', ogni utente è a conoscenza della sua posizione in 'users' */

int balance = 0;
int i, j;

int main(int argc, char** argv) {
    bzero(&act, sizeof(act)); /* bzero setta tutti i bytes a zero */

    /* Dico a sigaction (act) quale handler deve mandare in esecuzione */
    act.sa_sigaction = &hdl;
    /* Il campo SA_SIGINFO flag indica a sigaction() di usare il capo sa_sigaction, e non sa_handler. */
    act.sa_flags = SA_SIGINFO;

    /* »»»»»»»»»» SEGNALI »»»»»»»»»» */
    if (sigaction(SIGINT, &act, NULL) < 0) {
        perror(RED "[USER] Sigaction: Failed to assign SIGINT to custom handler" WHITE);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGTERM, &act, NULL) < 0) {
        perror(RED "[USER] Sigaction: Failed to assign SIGTERM to custom handler" WHITE);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGSEGV, &act, NULL) < 0) {
        perror(RED "[USER] Sigaction: Failed to assign SIGSEGV to custom handler" WHITE);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGUSR1, &act, NULL) < 0) {
        perror(RED "[USER] Sigaction: Failed to assign SIGUSR1 to custom handler" WHITE);
        exit(EXIT_FAILURE);
    }

    /* »»»»»»»»»» SEMAFORI »»»»»»»»»» */
    if ((semId = semget(ftok("./utils/private-key", 's'), 3, IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[USER] Semaphore pool creation failure" WHITE);
        exit(EXIT_FAILURE);
    }

    /* »»»»»»»»»» CODA DI MESSAGGI »»»»»»»»»» */
    if ((queueId = msgget(ftok("./utils/private-key", 'q'), IPC_CREAT | IPC_EXCL | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[USER] Message Queue creation failure" WHITE);
        exit(EXIT_FAILURE);
    }

    /* »»»»»»»»»» MEMORIE CONDIVISE »»»»»»»»»» */
    if ((shmLedgerId = shmget(ftok("./utils/private-key", 'l'), sizeof(ledger), IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[USER] Shared Memory creation failure [LEDGER]" WHITE);
        exit(EXIT_FAILURE);
    }

    mastro = (ledger*)shmat(shmLedgerId, NULL, 0);
    if (mastro == (void*)-1) {
        perror(RED "[USER] Shared Memory attach failure [LEDGER]" WHITE);
        exit(EXIT_FAILURE);
    }

    SO_USERS_NUM = atoi(argv[1]);
    if ((shmUsersId = shmget(ftok("./utils/private-key", 'u'), sizeof(process) * SO_USERS_NUM, IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[USER] Shared Memory creation failure [USERS]" WHITE);
        exit(EXIT_FAILURE);
    }

    users = (process*)shmat(shmUsersId, NULL, 0);
    if (users == (void*)-1) {
        perror(RED "[USER] Shared Memory attach failure [USERS]" WHITE);
        exit(EXIT_FAILURE);
    }

    SO_NODES_NUM = atoi(argv[2]);
    if ((shmNodesId = shmget(ftok("./utils/private-key", 'n'), sizeof(process) * SO_NODES_NUM, IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[USER] Shared Memory creation failure [NODES]" WHITE);
        exit(EXIT_FAILURE);
    }

    nodes = (process*)shmat(shmNodesId, NULL, 0);
    if (nodes == (void*)-1) {
        perror(RED "[USER] Shared Memory attach failure [NODES]" WHITE);
        exit(EXIT_FAILURE);
    }

    /* per sincronizzare i processi alla creazione iniziale, e dare loro il via con le operazioni */
    shmActiveProcessId = shmget(ftok("./utils/key-private", 'f'), sizeof(int), IPC_CREAT | 0644);
    if (shmActiveProcessId < 0) {
        perror(RED "[USER] Shared Memory creation failure [ActiveProcess]" WHITE);
        exit(EXIT_FAILURE);
    }

    activeProcess = (int*)shmat(shmActiveProcessId, NULL, 0);
    if (activeProcess == (void*)-1) {
        perror(RED "[USER] Shared Memory attach failure [ActiveProcess]" WHITE);
        exit(EXIT_FAILURE);
    }

    /* Il semaforo 2 viene utilizzato per accedere in mutua esclusione all'area di shared memory per sincronizzarsi */
    reserveSem(semId, 2);
    (*activeProcess)++;
    releaseSem(semId, 2);

#ifdef DEBUG
    reserveSem(semId, 1);
    printf("[%s%d%s] USER - Wrote +1 in shared\n", CYAN, getpid(), WHITE);
    releaseSem(semId, 1);
#endif

    /* Salvo le info necessarie per l'utente */
    SO_BUDGET_INIT = atoi(argv[3]);
    SO_REWARD = atoi(argv[4]);
    SO_MIN_TRANS_GEN_NSEC = atol(argv[5]);
    SO_MAX_TRANS_GEN_NSEC = atol(argv[6]);
    SO_RETRY = atoi(argv[7]);
    offset = atoi(argv[8]);

    srand(time(NULL) % getpid()); /* randomize seed */

    while (1) {
        reserveSem(semId, 0); /* scrivo nella memoria condivisa il bilancio aggiornato */
        (users + offset)->balance += balanceFromLedger(mastro);
        balance = (users + offset)->balance;
        releaseSem(semId, 0);

        if (balance >= 2) {
            transaction trans;
            trans = createTransaction();

            /* INVIO DELLA TRANSAZIONE */

            sleepTransactionGen();
        }
    }

    printf("\t%d Termino...\n", getpid());

    exit(EXIT_SUCCESS);
}

void sleepTransactionGen() {
    long transGenNsec, transGenSec;

    transGenNsec = (rand() % (SO_MAX_TRANS_GEN_NSEC - SO_MIN_TRANS_GEN_NSEC + 1)) + SO_MIN_TRANS_GEN_NSEC;

    if (transGenNsec > 999999999) {
        transGenSec = transGenNsec / 1000000000;    /* trovo la parte in secondi */
        transGenNsec -= (transGenSec * 1000000000); /* tolgo la parte in secondi dai nanosecondi */
    }

#ifdef DEBUG
    reserveSem(semId, 1);
    printf("%sSleep for: %lds %ldnsec\n%s", RED, transGenSec, transGenNsec, WHITE);
    releaseSem(semId, 1);
#endif

    request.tv_nsec = transGenNsec;
    request.tv_sec = transGenSec;
    remaining.tv_nsec = transGenNsec;
    remaining.tv_sec = transGenSec;

    if (nanosleep(&request, &remaining) < 0) {
        perror(RED "[USER] Nanosleep failed" WHITE);
        exit(EXIT_FAILURE);
    }
}

void hdl(int sig, siginfo_t* siginfo, void* context) {
    /**
	 * 	Questo è l'handler dei segnali, gestisce i segnali:
	 *  	-	SIGINT
	 *  	-	SIGTERM
	 *  	-	SIGSEGV
     *      -   SIGUSR1
	 **/
    switch (sig) {
        case SIGINT:
            releaseSem(semId, 0);
            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            shmdt(activeProcess);
            exit(EXIT_SUCCESS);

        case SIGTERM:
            releaseSem(semId, 0);
            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            shmdt(activeProcess);
            exit(EXIT_FAILURE);

        case SIGSEGV:
            releaseSem(semId, 0);
            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            shmdt(activeProcess);
            error("SEGMENTATION VIOLATION [USER]");

        case SIGUSR1: {
            transaction trans;
            reserveSem(semId, 1);
            printf("\n[ %sSIGUSR1%s ] Transaction creation due to SIGUSR1\n", YELLOW, WHITE);
            releaseSem(semId, 1);
            trans = createTransaction();
            sendTransaction(trans);
            break;
        }
    }
}

void sendTransaction(transaction trans) {
    int nodeReceiver, i = 0;
    message* mex;

    nodeReceiver = (rand() % SO_NODES_NUM); /* estraggo randomicamente una posizione tra 0 e SO_NODES_NUM escluso */

    mex->node = (nodes + nodeReceiver)->pid;
    mex->transaction = trans;

    while (i < SO_RETRY) {
        if (msgsnd(queueId, mex, sizeof(transaction) - sizeof(pid_t), 0) < 0) {
            ++i;
            /* perror(RED "[USER] Error in msgsnd()" WHITE);
            exit(EXIT_FAILURE); */
        }
    }

    if (i == SO_RETRY) { /* non è riuscito ad inviare la transazione */
        releaseSem(semId, 0);
        kill(getppid(), SIGUSR1);
        shmdt(mastro);
        shmdt(users);
        shmdt(nodes);
        shmdt(activeProcess);
        exit(EXIT_FAILURE);
    }
}

int balanceFromLedger() {
    int i, j;
    int balance = 0;
    for (i = 0; i < mastro->size; ++i) {
        for (j = 0; j < mastro->block[i].size; ++j) {
            if (mastro->block[i].transaction[j].sender == getpid()) {
                balance -= mastro->block[i].transaction[j].quantity;
            }
            if (mastro->block[i].transaction[j].receiver == getpid()) {
                balance += mastro->block[i].transaction[j].quantity;
            }
        }
    }
    return balance;
}

transaction createTransaction() {
    transaction trans;
    int userReceiver, amount, nodeReward;

    /* estrazione processo utente destinatario */
    do {
        userReceiver = (rand() % SO_USERS_NUM); /* estraggo randomicamente una posizione tra 0 e SO_USERS_NUM escluso */
    } while (userReceiver == offset);           /* cerco la posizione random finché non è diversa dalla posizione dell'utente corrente */

    /* estrazione della quantità da inviare */
    amount = (rand() % (balance - 1)) + 2; /* estraggo un numero compreso tra 2 e balance ==> estraggo un numero compreso tra 0 e balance-1 escluso ==> tra 0 e balance-2 e sommo 2 */

    nodeReward = (SO_REWARD * amount) / 100; /* calcolo del reward per il nodo */

    clock_gettime(CLOCK_MONOTONIC, &tp);
    trans.timestamp = (tp.tv_sec * 1000000000) + tp.tv_nsec; /* in questo modo ho creato un numero identificatore unico per questa transazione */
    trans.sender = getpid();
    trans.receiver = (users + userReceiver)->pid;
    trans.reward = (nodeReward < 1) ? 1 : nodeReward;
    trans.quantity = amount - ((nodeReward < 1) ? 1 : nodeReward);

#ifdef DEBUG
    reserveSem(semId, 1);
    printTransaction(trans);
    releaseSem(semId, 1);
#endif

    reserveSem(semId, 0);
    (users + offset)->balance -= (amount - nodeReward);
    releaseSem(semId, 0);

    return trans;
}