#include "master.h"

#include <time.h>

/* #define DEBUG */

/* »»»»»»»»»» VARIABILI di Configurazione »»»»»»»»»» */
int SO_USERS_NUM;           /* numero di processi utente che possono inviare denaro ad altri utenti attraverso una transazione */
int SO_NODES_NUM;           /* numero di processi nodo che elaborano, a pagamento, le transazioni ricevute */
int SO_REWARD;              /* la percentuale di reward pagata da ogni utente per il processamento di una transazione */
int SO_MIN_TRANS_GEN_NSEC;  /* minimo valore del tempo (espresso in nanosecondi) che trascorre fra la generazione di una transazione e la seguente da parte di un utente */
int SO_MAX_TRANS_GEN_NSEC;  /* massimo valore del tempo (espresso in nanosecondi) che trascorre fra la generazione di una transazione e la seguente da parte di un utente */
int SO_RETRY;               /* numero di tentativi che un processo utente ha per inviare una transazione */
int SO_TP_SIZE;             /* numero massimo di transazioni nella transaction pool dei processi nodo */
int SO_MIN_TRANS_PROC_NSEC; /* minimo valore del tempo simulato (espresso in nanosecondi) di processamento di un blocco da parte di un nodo */
int SO_MAX_TRANS_PROC_NSEC; /* massimo valore del tempo simulato (espresso in nanosecondi) di processamento di un blocco da parte di un nodo */
int SO_BUDGET_INIT;         /* budget iniziale di ciascun processo utente */
int SO_SIM_SEC;             /* durata della simulazione (in secondi) */
int SO_FRIENDS_NUM;         /* IMPORTANT numero di nodi amici dei processi nodo (solo per la versione full) */
int SO_HOPS;                /* IMPORTANT numero massimo di salti massimo che una transazione può effettuare quando la transaction pool di un nodo è piena (solo per la versione full) */

/* »»»»»»»»»» Definizione Metodi »»»»»»»»»» */

void readConfigFile(char*);
void printConfigVal();

int main(int argc, char** argv) {
    key_t private_key;
    int msgId;
    int ledgerId;
    ledger* mastro;
    char* info[3] = {"utente", "Ciao", NULL};
    int i, j;
    struct timespec tp;
    pid_t pid;

    if (argc != 2) /* controllo sul numero di argomenti */
        error("Usage: ./master <filepath>\n\n\t<filepath>: Config's file path.");

    readConfigFile(argv[1]); /* lettura del file di configurazione */

#ifdef DEBUG
    printConfigVal();
#endif

    /* if ((private_key = ftok("./utils/private-key", 'a')) == -1)
        error("ftok() failed!");

    printf("%d\n", private_key);

    if ((msgId = msgget(private_key, IPC_CREAT | 0666)) == -1)
        error("msgget() failed!");

    printf("%d\n", msgId); */

    /* Creazione Libro Mastro */
    if ((ledgerId = shmget(ftok("../utils/private-key", 'l'), sizeof(ledger), IPC_CREAT | 0644)) < 0)
        error("Ledger creation. (shmget)");

    mastro = (ledger*)shmat(ledgerId, NULL, 0);
    if (mastro == (void*)-1)
        error("Ledger attaching. (shmat)");

    mastro->size = 1;          /* grandezza iniziale del libro mastro: 1 -> il primo blocco con le transazioni iniziali */
    mastro->block[0].size = 0; /* inzializzazione del primo blocco: 0 -> vuoto */
    j = 0;
    /*
    if (shmctl(ledgerId, IPC_RMID, NULL) < 0)
        error("Ledger delete. (shmctl(..., IPC_RMID, ...))"); */

    for (i = 0; i < SO_USERS_NUM; ++i) {
        switch (pid = fork()) {
            case -1:
                break;

            case 0:
                printf("PID figlio: %d\n", getpid());
                execve("./utente", info, __environ);
                exit(EXIT_SUCCESS);

            default:

                clock_gettime(CLOCK_MONOTONIC, &tp);
                mastro->block[0].transaction[j].timestamp = (tp.tv_sec * 1000000000) + tp.tv_nsec;
                mastro->block[0].transaction[j].sender = specialSender;
                mastro->block[0].transaction[j].receiver = pid;
                mastro->block[0].transaction[j].quantity = SO_BUDGET_INIT;
                mastro->block[0].transaction[j].reward = 0;
                mastro->block[0].size++;
                j++;

                break;
        }
    }

    for (i = 0; i < SO_USERS_NUM; ++i)
        wait(NULL);

    printLedger(mastro);

    if (shmdt(mastro) < 0)
        error("Ledger detaching. (shmdt)");
}

/* legge dal percorso in argv[1] e inizializza le variabili di configurazione */
void readConfigFile(char* path) {
    FILE* fp;                            /* puntatore a file */
    if ((fp = fopen(path, "r")) == NULL) /* apertura del file di configurazione in sola lettura */
        error("File Not Found!");

    fscanf(fp, "SO_USERS_NUM: %d\n", &SO_USERS_NUM);                            /* numero di processi utente che possono inviare denaro ad altri utenti attraverso una transazione */
    fscanf(fp, "SO_NODES_NUM: %d\n", &SO_NODES_NUM);                            /* numero di processi nodo che elaborano, a pagamento, le transazioni ricevute */
    fscanf(fp, "SO_REWARD: %d\n", &SO_REWARD);                                  /* la percentuale di reward pagata da ogni utente per il processamento di una transazione */
    fscanf(fp, "SO_MIN_TRANS_GEN_NSEC [nsec]: %d\n", &SO_MIN_TRANS_GEN_NSEC);   /* minimo valore del tempo (espresso in nanosecondi) che trascorre fra la generazione di una transazione e la seguente da parte di un utente */
    fscanf(fp, "SO_MAX_TRANS_GEN_NSEC [nsec]: %d\n", &SO_MAX_TRANS_GEN_NSEC);   /* massimo valore del tempo (espresso in nanosecondi) che trascorre fra la generazione di una transazione e la seguente da parte di un utente */
    fscanf(fp, "SO_RETRY: %d\n", &SO_RETRY);                                    /* numero di tentativi che un processo utente ha per inviare una transazione */
    fscanf(fp, "SO_TP_SIZE: %d\n", &SO_TP_SIZE);                                /* numero massimo di transazioni nella transaction pool dei processi nodo */
    fscanf(fp, "SO_MIN_TRANS_PROC_NSEC [nsec]: %d\n", &SO_MIN_TRANS_PROC_NSEC); /* minimo valore del tempo simulato (espresso in nanosecondi) di processamento di un blocco da parte di un nodo */
    fscanf(fp, "SO_MAX_TRANS_PROC_NSEC [nsec]: %d\n", &SO_MAX_TRANS_PROC_NSEC); /* massimo valore del tempo simulato (espresso in nanosecondi) di processamento di un blocco da parte di un nodo */
    fscanf(fp, "SO_BUDGET_INIT: %d\n", &SO_BUDGET_INIT);                        /* budget iniziale di ciascun processo utente */
    fscanf(fp, "SO_SIM_SEC: %d\n", &SO_SIM_SEC);                                /* durata della simulazione (in secondi) */
    fscanf(fp, "SO_FRIENDS_NUM: %d\n", &SO_FRIENDS_NUM);                        /* IMPORTANT numero di nodi amici dei processi nodo (solo per la versione full) */
    fscanf(fp, "SO_HOPS: %d\n", &SO_HOPS);                                      /* IMPORTANT numero massimo di salti massimo che una transazione può effettuare quando la transaction pool di un nodo è piena (solo per la versione full) */

    fclose(fp); /* chiusura del file */
}

/* scrive su stdout il valore delle variabili di configurazione */
void printConfigVal() {
    printf("\nSO_USERS_NUM: %d\n", SO_USERS_NUM);
    printf("SO_NODES_NUM: %d\n", SO_NODES_NUM);
    printf("SO_REWARD: %d\n", SO_REWARD);
    printf("SO_MIN_TRANS_GEN_NSEC [nsec]: %d\n", SO_MIN_TRANS_GEN_NSEC);
    printf("SO_MAX_TRANS_GEN_NSEC [nsec]: %d\n", SO_MAX_TRANS_GEN_NSEC);
    printf("SO_RETRY: %d\n", SO_RETRY);
    printf("SO_TP_SIZE: %d\n", SO_TP_SIZE);
    printf("SO_MIN_TRANS_PROC_NSEC [nsec]: %d\n", SO_MIN_TRANS_PROC_NSEC);
    printf("SO_MAX_TRANS_PROC_NSEC [nsec]: %d\n", SO_MAX_TRANS_PROC_NSEC);
    printf("SO_BUDGET_INIT: %d\n", SO_BUDGET_INIT);
    printf("SO_SIM_SEC: %d\n", SO_SIM_SEC);
    printf("SO_FRIENDS_NUM: %d\n", SO_FRIENDS_NUM);
    printf("SO_HOPS: %d\n", SO_HOPS);
    printf(" #SO_BLOCK_SIZE: %d\n", SO_BLOCK_SIZE);
    printf(" #SO_REGISTRY_SIZE: %d\n\n", SO_REGISTRY_SIZE);
}