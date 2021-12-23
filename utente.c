#include "master.h"

/* #define DEBUG */

/* Gestore dei segnali */
void hdl(int, siginfo_t*, void*);

/** Calcola il bilancio delle transazioni presenti nel Libro Mastro 
 * 
 * @param l puntatore alla struttura Libro Mastro
 * @return il bilancio (entrate/uscite) delle transazioni fatte dall'utente
 */
int balanceFromLedger(ledger*);

/** Cerca l'offset che contiene le informazioni riguardanti l'attuale utente nella memoria condivisa 'users'
 * 
 * @return l'offset da 'users' se l'utente esiste, -1 se non esiste
 */
int whoAmI();

struct sigaction act;

int semId;              /* ftok(..., 's') => 's': semaphore */
int shmLedgerId;        /* ftok(..., 'l') => 'l': ledger */
int shmUsersId;         /* ftok(..., 'u') => 'u': users */
int shmNodesId;         /* ftok(..., 'n') => 'n': nodes */
int shmActiveProcessId; /* ftok(..., 'p') => 'p': process */

ledger* mastro;     /* informazioni sul libro mastro in memoria condivisa */
process* users;     /* informazioni sugli utenti in memoria condivisa */
process* nodes;     /* informazione sui nodi in memoria condivisa */
int* activeProcess; /* conteggio dei processi attivi in shared memory */

int usersNum;     /* numero di utenti nella simulazione */
int nodesNum;     /* numero di nodi nella simulazione */
int budgetInit;   /* budget iniziale di un utente */
int reward;       /* reward da applicare alla transazione */
long minTransGen; /* minimo valore del tempo (espresso in nanosecondi) che trascorre fra la generazione di una transazione e la seguente da parte di un utente */
long maxTransGen; /* massimo valore del tempo (espresso in nanosecondi) che trascorre fra la generazione di una transazione e la seguente da parte di un utente */
int retry;        /* numero massimo di fallimenti consecutivi nella generazione di transazioni dopo cui un processo utente termina */

int offset;
int balance = 0;
int i, j;

int main(int argc, char** argv) {
    bzero(&act, sizeof(act)); /* bzero setta tutti i bytes a zero */

    /* Dico a sigaction (act) quale handler deve mandare in esecuzione */
    act.sa_sigaction = &hdl;
    /* Il campo SA_SIGINFO flag indica a sigaction() di usare il capo sa_sigaction, e non sa_handler. */
    act.sa_flags = SA_SIGINFO;

    /* SEGNALI */
    if (sigaction(SIGINT, &act, NULL) < 0) {
        perror(RED "Sigaction: Failed to assign SIGINT to custom handler" WHITE);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGTERM, &act, NULL) < 0) {
        perror(RED "Sigaction: Failed to assign SIGTERM to custom handler" WHITE);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGSEGV, &act, NULL) < 0) {
        perror(RED "Sigaction: Failed to assign SIGSEGV to custom handler" WHITE);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGUSR1, &act, NULL) < 0) {
        perror(RED "Sigaction: Failed to assign SIGUSR1 to custom handler" WHITE);
        exit(EXIT_FAILURE);
    }

    /* SEMAFORI */
    if ((semId = semget(ftok("./utils/private-key", 's'), 3, IPC_CREAT | 0644)) < 0) {
        perror(RED "Semaphore pool creation failure" WHITE);
        exit(EXIT_FAILURE);
    }

    /* MEMORIE CONDIVISE */
    if ((shmLedgerId = shmget(ftok("./utils/private-key", 'l'), sizeof(ledger), IPC_CREAT | 0644)) < 0) {
        perror(RED "Shared Memory creation failure [LEDGER]" WHITE);
        exit(EXIT_FAILURE);
    }

    mastro = (ledger*)shmat(shmLedgerId, NULL, 0);
    if (mastro == (void*)-1) {
        perror(RED "Shared Memory attach failure [LEDGER]" WHITE);
        exit(EXIT_FAILURE);
    }

    usersNum = atoi(argv[1]);
    if ((shmUsersId = shmget(ftok("./utils/private-key", 'u'), sizeof(process) * usersNum, IPC_CREAT | 0644)) < 0) {
        perror(RED "Shared Memory creation failure [USERS]" WHITE);
        exit(EXIT_FAILURE);
    }

    users = (process*)shmat(shmUsersId, NULL, 0);
    if (users == (void*)-1) {
        perror(RED "Shared Memory attach failure [USERS]" WHITE);
        exit(EXIT_FAILURE);
    }

    nodesNum = atoi(argv[2]);
    if ((shmNodesId = shmget(ftok("./utils/private-key", 'n'), sizeof(process) * nodesNum, IPC_CREAT | 0644)) < 0) {
        perror(RED "Shared Memory creation failure [NODES]" WHITE);
        exit(EXIT_FAILURE);
    }

    nodes = (process*)shmat(shmNodesId, NULL, 0);
    if (nodes == (void*)-1) {
        perror(RED "Shared Memory attach failure [NODES]" WHITE);
        exit(EXIT_FAILURE);
    }

    /* per sincronizzare i processi alla creazione iniziale, e dare loro il via con le operazioni */
    shmActiveProcessId = shmget(ftok("./utils/key-private", 'f'), sizeof(int), IPC_CREAT | 0644);
    if (shmActiveProcessId < 0) {
        perror(RED "Shared Memory creation failure [ActiveProcess]" WHITE);
        exit(EXIT_FAILURE);
    }

    activeProcess = (int*)shmat(shmActiveProcessId, NULL, 0);
    if (activeProcess == (void*)-1) {
        perror(RED "Shared Memory attach failure [ActiveProcess]" WHITE);
        exit(EXIT_FAILURE);
    }

    /* Il semaforo 2 viene utilizzato per accedere in mutua esclusione all'area di shared memory per sincronizzarsi */
    reserveSem(semId, 2);
    printf("Processi attivi: %d\n", (*activeProcess) + 1);
    (*activeProcess)++;
    releaseSem(semId, 2);

#ifdef DEBUG
    printf("[%s%d%s] USER - Wrote +1 in shared\n", CYAN, getpid(), WHITE);
#endif

    /*  balance = balanceFromLedger(mastro);
    printf("[%d] BILANCIO: %d\n", getpid(), balance);

    if (shmdt(mastro) < 0)
        error("Ledger detaching. (shmdt)"); */

    reserveSem(semId, 0); /* aspetto il via dal master */

    /* cerco l'offset che contiene le informazioni riguardanti l'attuale utente nella memoria condivisa 'users' */
    if ((offset = whoAmI()) < 0) {
        printf("%sUser %d not found!%s\n", RED, getpid(), WHITE);
    }

    releaseSem(semId, 0);

    printf("%d\n", getpid());
    while (1) {
        fflush(stdout);
        printf("#");
        sleep(1);
    }

    printf("\t%d Termino...\n", getpid());

    exit(EXIT_SUCCESS);
}

int whoAmI() {
    int found = -1;
    int i;
    for (i = 0; found == -1 && i < usersNum; ++i) {
        if ((users + i)->pid == getpid()) {
            found = i;
        }
    }
    return found;
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
            printf("QUI\n");
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

        case SIGUSR1:
            printf("%ld\n", (long)siginfo->si_pid); /* (long)siginfo->si_pid ==> PID di chi invia il segnale */
            printf("\n[ %sSIGUSR1%s ] Simulation interrupted due to SIGUSR1\n", YELLOW, WHITE);
            exit(EXIT_FAILURE);
            break;
    }
}

int balanceFromLedger(ledger* l) {
    int i, j;
    int balance = 0;
    for (i = 0; i < l->size; ++i) {
        for (j = 0; j < l->block[i].size; ++j) {
            if (l->block[i].transaction[j].sender == getpid()) {
                balance -= l->block[i].transaction[j].quantity;
            }
            if (l->block[i].transaction[j].receiver == getpid()) {
                balance += l->block[i].transaction[j].quantity;
            }
        }
    }
    return balance;
}