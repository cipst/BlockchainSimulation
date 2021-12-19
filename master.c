#include "master.h"

#include <string.h>
#include <time.h>

/* #define DEBUG */

/* »»»»»»»»»» VARIABILI di Configurazione »»»»»»»»»» */
int SO_USERS_NUM;            /* numero di processi utente che possono inviare denaro ad altri utenti attraverso una transazione */
int SO_NODES_NUM;            /* numero di processi nodo che elaborano, a pagamento, le transazioni ricevute */
int SO_REWARD;               /* la percentuale di reward pagata da ogni utente per il processamento di una transazione */
long SO_MIN_TRANS_GEN_NSEC;  /* minimo valore del tempo (espresso in nanosecondi) che trascorre fra la generazione di una transazione e la seguente da parte di un utente */
long SO_MAX_TRANS_GEN_NSEC;  /* massimo valore del tempo (espresso in nanosecondi) che trascorre fra la generazione di una transazione e la seguente da parte di un utente */
int SO_RETRY;                /* numero di tentativi che un processo utente ha per inviare una transazione */
int SO_TP_SIZE;              /* numero massimo di transazioni nella transaction pool dei processi nodo */
long SO_MIN_TRANS_PROC_NSEC; /* minimo valore del tempo simulato (espresso in nanosecondi) di processamento di un blocco da parte di un nodo */
long SO_MAX_TRANS_PROC_NSEC; /* massimo valore del tempo simulato (espresso in nanosecondi) di processamento di un blocco da parte di un nodo */
int SO_BUDGET_INIT;          /* budget iniziale di ciascun processo utente */
time_t SO_SIM_SEC;           /* durata della simulazione (in secondi) */
int SO_FRIENDS_NUM;          /* IMPORTANT numero di nodi amici dei processi nodo (solo per la versione full) */
int SO_HOPS;                 /* IMPORTANT numero massimo di salti massimo che una transazione può effettuare quando la transaction pool di un nodo è piena (solo per la versione full) */

/* »»»»»»»»»» Definizione Metodi »»»»»»»»»» */

void hdl(int, siginfo_t*, void*);
void killall(int);
void print_ipc_status();
void print_stats();
void readConfigFile(char*);
void printConfigVal();

struct sigaction act;
sigset_t sigMask; /* inizializzo la maschera dei segnali da ignorare */
struct timespec tp;

int semId;              /* ftok(..., 's') => 's': semaphore */
int shmLedgerId;        /* ftok(..., 'l') => 'l': ledger */
int shmUsersId;         /* ftok(..., 'u') => 'u': users */
int shmNodesId;         /* ftok(..., 'n') => 'n': nodes */
int shmActiveProcessId; /* ftok(..., 'p') => 'p': process */

ledger* mastro;     /* informazioni sul libro mastro in memoria condivisa */
process* users;     /* informazioni sugli utenti in memoria condivisa */
process* nodes;     /* informazione sui nodi in memoria condivisa */
int* activeProcess; /* conteggio dei processi attivi in shared memory */

int totalUsers = 0;
int totalNodes = 0;

pid_t master_pid;
char* info[3] = {"utente", "Ciao", NULL};
int i, j;
pid_t pid;

int main(int argc, char** argv) {
    if (argc != 2) /* controllo sul numero di argomenti */
        error("Usage: ./master <filepath>\n\n\t<filepath>: Config's file path.");

    readConfigFile(argv[1]); /* lettura del file di configurazione */

#ifdef DEBUG
    printConfigVal();
#endif

    bzero(&act, sizeof(act)); /* bzero setta tutti i bytes a zero */

    /* Dico a sigaction (act) quale handler deve mandare in esecuzione */
    act.sa_sigaction = &hdl;
    /* Il campo SA_SIGINFO flag indica a sigaction() di usare il capo sa_sigaction, e non sa_handler. */
    act.sa_flags = SA_SIGINFO;

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
    if (sigaction(SIGALRM, &act, NULL) < 0) {
        perror(RED "Sigaction: Failed to assign SIGALRM to custom handler" WHITE);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGUSR1, &act, NULL) < 0) {
        perror(RED "Sigaction: Failed to assign SIGUSR1 to custom handler" WHITE);
        exit(EXIT_FAILURE);
    }

    if (sigemptyset(&sigMask) == -1)
        error("Error in sigMask");
    if (sigaddset(&sigMask, SIGALRM) == -1)
        error("Error adding SIGALRM to sigMask");

    if ((semId = semget(ftok("./utils/private-key", 's'), 3, IPC_CREAT | IPC_EXCL | 0644)) < 0) {
        perror(RED "Semaphore pool creation failure" WHITE);
        exit(EXIT_FAILURE);
    }

    if (initSemAvailable(semId, 0) < 0) { /* semaforo di mutua esclusione per scrivere in shared memory */
        perror(RED "Failed to initialize semaphore 0" WHITE);
        exit(EXIT_FAILURE);
    }

    if (initSemAvailable(semId, 1) < 0) { /* semaforo utilizzato per non sovrapporre le stampe a video */
        perror(RED "Failed to initialize semaphore 1" WHITE);
        exit(EXIT_FAILURE);
    }

    if (initSemAvailable(semId, 2) < 0) { /* semaforo utilizzato per l'accesso alla shared memory di sincronizzazione */
        perror(RED "Failed to initialize semaphore 2" WHITE);
        exit(EXIT_FAILURE);
    }

    if ((shmLedgerId = shmget(ftok("./utils/private-key", 'l'), sizeof(ledger), IPC_CREAT | IPC_EXCL | 0644)) < 0) {
        perror(RED "Shared Memory creation failure [LEDGER]" WHITE);
        exit(EXIT_FAILURE);
    }

    mastro = (ledger*)shmat(shmLedgerId, NULL, 0);
    if (mastro == (void*)-1) {
        perror(RED "Shared Memory attach failure [LEDGER]" WHITE);
        exit(EXIT_FAILURE);
    }

    if ((shmUsersId = shmget(ftok("./utils/private-key", 'u'), sizeof(process) * SO_USERS_NUM, IPC_CREAT | IPC_EXCL | 0644)) < 0) {
        perror(RED "Shared Memory creation failure [USERS]" WHITE);
        exit(EXIT_FAILURE);
    }

    users = (process*)shmat(shmUsersId, NULL, 0);
    if (users == (void*)-1) {
        perror(RED "Shared Memory attach failure [USERS]" WHITE);
        exit(EXIT_FAILURE);
    }

    if ((shmNodesId = shmget(ftok("./utils/private-key", 'n'), sizeof(process) * SO_NODES_NUM, IPC_CREAT | IPC_EXCL | 0644)) < 0) {
        perror(RED "Shared Memory creation failure [NODES]" WHITE);
        exit(EXIT_FAILURE);
    }

    nodes = (process*)shmat(shmNodesId, NULL, 0);
    if (nodes == (void*)-1) {
        perror(RED "Shared Memory attach failure [NODES]" WHITE);
        exit(EXIT_FAILURE);
    }

    /* per sincronizzare i processi alla creazione iniziale, e dare loro il via con le operazioni */
    shmActiveProcessId = shmget(ftok("./utils/key-private", 'f'), sizeof(int), IPC_CREAT | IPC_EXCL | 0644);
    if (shmActiveProcessId < 0) {
        perror(RED "Shared Memory creation failure [ActiveProcess]" WHITE);
        exit(EXIT_FAILURE);
    }

    activeProcess = (int*)shmat(shmActiveProcessId, NULL, 0);
    if (activeProcess == (void*)-1) {
        perror(RED "Shared Memory attach failure [ActiveProcess]" WHITE);
        exit(EXIT_FAILURE);
    }

    /* Inizializzo activeProcess a 0 => nessun processo 'utente' è attivo */
    /* Il semaforo 2 viene utilizzato per accedere in mutua esclusione all'area di shared memory per sincronizzarsi */
    reserveSem(semId, 2);
    *activeProcess = 0;
    releaseSem(semId, 2);

    print_ipc_status(); /* stato degli oggetti IPC */

    master_pid = getpid();

    mastro->size = 1;                     /* inizializzo il libro mastro a 1 => un blocco contenente le transazione per processi utente */
    mastro->block[0].size = SO_USERS_NUM; /* nel primo blocco ci sono SO_USERS_NUM transazioni */

    for (i = 0; i < SO_USERS_NUM; ++i) {
        clock_gettime(CLOCK_MONOTONIC, &tp);
        mastro->block[0].transaction[i].timestamp = (tp.tv_sec * 1000000000) + tp.tv_nsec;
        mastro->block[0].transaction[i].quantity = SO_BUDGET_INIT;
        mastro->block[0].transaction[i].sender = specialSender;
        mastro->block[0].transaction[i].receiver = i;
        mastro->block[0].transaction[i].reward = SO_REWARD;
    }

    /* alarm(1); */

    printLedger(mastro);

    while (1) {
        fflush(stdout);
        printf("#");
        sleep(1);
    }
}

void hdl(int sig, siginfo_t* siginfo, void* context) {
    /**
	 * 	Questo è l'handler dei segnali, gestisce i segnali:
	 * 		-	SIGALRM
	 *  	-	SIGINT
	 *  	-	SIGTERM
	 *  	-	SIGSEGV
     *      -   SIGUSR1
	 **/

    switch (sig) {
        case SIGALRM:
            if (sigaction(SIGALRM, &act, NULL) < 0) {
                perror(RED "Sigaction: Failed to assign SIGALRM to custom handler" WHITE);
                exit(EXIT_FAILURE);
            }
            printf("%ld\n", SO_SIM_SEC);

            printf("\n%sSO_SIM_SEC%s expired - Terminating simulation...", YELLOW, WHITE);
            killall(SIGINT);

            print_stats();
            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            shmdt(activeProcess);

            system("./ipcrm.sh");
            exit(EXIT_SUCCESS);
            break;

        case SIGINT:
#ifdef DEBUG
            printf("\n[ %sSIGINT%s ] %ld sended SIGINT\n", YELLOW, WHITE, (long)siginfo->si_pid);
#endif
            if ((long)siginfo->si_pid != master_pid) {
                signal(SIGINT, SIG_IGN);
                kill(0, SIGINT);
                sigaction(SIGINT, &act, NULL);

                print_stats();
                shmdt(mastro);
                shmdt(users);
                shmdt(nodes);
                shmdt(activeProcess);

                system("./ipcrm.sh");
                printf("[ %sSIGINT%s ] Simulation interrupted\n", YELLOW, WHITE);
                exit(EXIT_SUCCESS);
            }
            break;

        case SIGTERM:
            kill(0, SIGINT);
            break;

        case SIGSEGV:
            printf("\n[ %sSIGSEGV%s ] Simulation interrupted due to SIGSEGV\n", RED, WHITE);
            killall(SIGINT);
            exit(EXIT_FAILURE);
            break;

        case SIGUSR1:
            printf("%ld\n", (long)siginfo->si_pid); /* (long)siginfo->si_pid ==> PID di chi invia il segnale */
            printf("\n[ %sSIGUSR1%s ] Simulation interrupted due to SIGUSR1\n", YELLOW, WHITE);
            exit(EXIT_FAILURE);
            break;
    }
}

void killall(int sig) {
    /**
	 *  Funzione che uccide tutti i processi in shared memory
	 *  Invia il segnale passatogli come argomento a tutti i processi presenti nella shared memory;
	 * 			questa funzione scorre entrambe le shared memory.	
	 **/
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

void print_ipc_status() {
    /**
	 * 	Funzione utilizzata per stampare a video gli ID degli oggetti IPC
	 * 		Parametri:
	 * 			-	semId: è l'id del semaforo
	 * 			-	shmLedgerId: è l'id dell'area di shared memory dedicata al libro mastro (creata con shmget)
	 * 			-	shmUsersId: è l'id dell'area di shared memory dedicata ai processi Utente (creata con shmget)
	 * 			-	shmNodesId: è l'id dell'area di shared memory dedicata ai processi Nodo (creata con shmget)
     *          -   shmActiveProcessId: è l'id dell'area di shared memory usata dal gestore per sincronizzare l'avvio di tutti i processi
	 **/

    const char* aux = GREEN "IPC" WHITE;
    printf("\n[ %s ] Printing ipc objects status\n", aux);
    printf("\t[ %s ] semid: %d\n", aux, semId);
    printf("\t[ %s ] shmLedgerId: %d\n", aux, shmLedgerId);
    printf("\t[ %s ] shmUsersId: %d\n", aux, shmUsersId);
    printf("\t[ %s ] shmNodesId: %d\n", aux, shmNodesId);
    printf("\t[ %s ] shmActiveProcessId: %d\n\n", aux, shmActiveProcessId);
}

void print_stats() {
    /**
	 * Questa è una funzione che stampa tutte le informazioni ritenute utili prima della terminazione del programma
	 * 		Vengono stampate a video:
	 * 			-	
	 **/

    char aux[] = MAGENTA "STATS" WHITE;
    printf("\n[ %s ] Some stats of the simulation\n", aux);
    printf("\t[ %s ] Number of blocks in the Ledger: %d/%d\n", aux, mastro->size, SO_REGISTRY_SIZE);
    printf("\t[ %s ] Master PID: %d\n", aux, master_pid);
}

/* legge dal percorso in argv[1], inizializza le variabili di configurazione e controlla che i valori siano corretti */
void readConfigFile(char* path) {
    FILE* fp;                            /* puntatore a file */
    if ((fp = fopen(path, "r")) == NULL) /* apertura del file di configurazione in sola lettura */
        error("File Not Found!");

    fscanf(fp, "SO_USERS_NUM: %d\n", &SO_USERS_NUM);                             /* numero di processi utente che possono inviare denaro ad altri utenti attraverso una transazione */
    fscanf(fp, "SO_NODES_NUM: %d\n", &SO_NODES_NUM);                             /* numero di processi nodo che elaborano, a pagamento, le transazioni ricevute */
    fscanf(fp, "SO_REWARD: %d\n", &SO_REWARD);                                   /* la percentuale di reward pagata da ogni utente per il processamento di una transazione */
    fscanf(fp, "SO_MIN_TRANS_GEN_NSEC [nsec]: %ld\n", &SO_MIN_TRANS_GEN_NSEC);   /* minimo valore del tempo (espresso in nanosecondi) che trascorre fra la generazione di una transazione e la seguente da parte di un utente */
    fscanf(fp, "SO_MAX_TRANS_GEN_NSEC [nsec]: %ld\n", &SO_MAX_TRANS_GEN_NSEC);   /* massimo valore del tempo (espresso in nanosecondi) che trascorre fra la generazione di una transazione e la seguente da parte di un utente */
    fscanf(fp, "SO_RETRY: %d\n", &SO_RETRY);                                     /* numero di tentativi che un processo utente ha per inviare una transazione */
    fscanf(fp, "SO_TP_SIZE: %d\n", &SO_TP_SIZE);                                 /* numero massimo di transazioni nella transaction pool dei processi nodo */
    fscanf(fp, "SO_MIN_TRANS_PROC_NSEC [nsec]: %ld\n", &SO_MIN_TRANS_PROC_NSEC); /* minimo valore del tempo simulato (espresso in nanosecondi) di processamento di un blocco da parte di un nodo */
    fscanf(fp, "SO_MAX_TRANS_PROC_NSEC [nsec]: %ld\n", &SO_MAX_TRANS_PROC_NSEC); /* massimo valore del tempo simulato (espresso in nanosecondi) di processamento di un blocco da parte di un nodo */
    fscanf(fp, "SO_BUDGET_INIT: %d\n", &SO_BUDGET_INIT);                         /* budget iniziale di ciascun processo utente */
    fscanf(fp, "SO_SIM_SEC: %ld\n", &SO_SIM_SEC);                                /* durata della simulazione (in secondi) */
    fscanf(fp, "SO_FRIENDS_NUM: %d\n", &SO_FRIENDS_NUM);                         /* IMPORTANT numero di nodi amici dei processi nodo (solo per la versione full) */
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

    if (SO_RETRY < 0)
        error("SO_RETRY must be greater than or equal to 0.");

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

    if (SO_FRIENDS_NUM < 0)
        error("SO_FRIENDS_NUM must be greater than or equal to 0.");

    if (SO_HOPS < 0)
        error("SO_HOPS must be greater than or equal to 0.");
}

/* scrive su stdout il valore delle variabili di configurazione */
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
    printf("SO_FRIENDS_NUM: %d\n", SO_FRIENDS_NUM);
    printf("SO_HOPS: %d\n", SO_HOPS);
    printf(" #SO_BLOCK_SIZE: %d\n", SO_BLOCK_SIZE);
    printf(" #SO_REGISTRY_SIZE: %d\n\n", SO_REGISTRY_SIZE);
}