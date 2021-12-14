#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "header.h"

#define SO_BLOCK_SIZE 5
#define SO_REGISTRY_SIZE 10

int main(int argc, char** argv) {
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
    FILE* fp;                   /* puntatore a file */
    key_t private_key;
    int msgId;
    int i;

    if (argc != 2) /* controllo sul numero di argomenti */
        error("Usage: ./master <filepath>\n\n\t<filepath>: Config's file path.");

    if ((fp = fopen(argv[1], "r")) == NULL) /* apertura del file di configurazione in sola lettura */
        error("File Not Found!");

    fscanf(fp, "SO_USERS_NUM: %d\n", &SO_USERS_NUM);                     /* numero di processi utente che possono inviare denaro ad altri utenti attraverso una transazione */
    fscanf(fp, "SO_NODES_NUM: %d\n", &SO_NODES_NUM);                     /* numero di processi nodo che elaborano, a pagamento, le transazioni ricevute */
    fscanf(fp, "SO_REWARD: %d\n", &SO_REWARD);                           /* la percentuale di reward pagata da ogni utente per il processamento di una transazione */
    fscanf(fp, "SO_MIN_TRANS_GEN_NSEC: %d\n", &SO_MIN_TRANS_GEN_NSEC);   /* minimo valore del tempo (espresso in nanosecondi) che trascorre fra la generazione di una transazione e la seguente da parte di un utente */
    fscanf(fp, "SO_MAX_TRANS_GEN_NSEC: %d\n", &SO_MAX_TRANS_GEN_NSEC);   /* massimo valore del tempo (espresso in nanosecondi) che trascorre fra la generazione di una transazione e la seguente da parte di un utente */
    fscanf(fp, "SO_RETRY: %d\n", &SO_RETRY);                             /* numero di tentativi che un processo utente ha per inviare una transazione */
    fscanf(fp, "SO_TP_SIZE: %d\n", &SO_TP_SIZE);                         /* numero massimo di transazioni nella transaction pool dei processi nodo */
    fscanf(fp, "SO_MIN_TRANS_PROC_NSEC: %d\n", &SO_MIN_TRANS_PROC_NSEC); /* minimo valore del tempo simulato (espresso in nanosecondi) di processamento di un blocco da parte di un nodo */
    fscanf(fp, "SO_MAX_TRANS_PROC_NSEC: %d\n", &SO_MAX_TRANS_PROC_NSEC); /* massimo valore del tempo simulato (espresso in nanosecondi) di processamento di un blocco da parte di un nodo */
    fscanf(fp, "SO_BUDGET_INIT: %d\n", &SO_BUDGET_INIT);                 /* budget iniziale di ciascun processo utente */
    fscanf(fp, "SO_SIM_SEC: %d\n", &SO_SIM_SEC);                         /* durata della simulazione (in secondi) */
    fscanf(fp, "SO_FRIENDS_NUM: %d\n", &SO_FRIENDS_NUM);                 /* IMPORTANT numero di nodi amici dei processi nodo (solo per la versione full) */
    fscanf(fp, "SO_HOPS: %d\n", &SO_HOPS);                               /* IMPORTANT numero massimo di salti massimo che una transazione può effettuare quando la transaction pool di un nodo è piena (solo per la versione full) */

    fclose(fp); /* chiusura del file */

    printf("\n\nSO_USERS_NUM: %d\n", SO_USERS_NUM);
    printf("SO_NODES_NUM: %d\n", SO_NODES_NUM);
    printf("SO_REWARD: %d\n", SO_REWARD);
    printf("SO_MIN_TRANS_GEN_NSEC: %d\n", SO_MIN_TRANS_GEN_NSEC);
    printf("SO_MAX_TRANS_GEN_NSEC: %d\n", SO_MAX_TRANS_GEN_NSEC);
    printf("SO_RETRY: %d\n", SO_RETRY);
    printf("SO_TP_SIZE: %d\n", SO_TP_SIZE);
    printf("#SO_BLOCK_SIZE: %d\n", SO_BLOCK_SIZE);
    printf("SO_MIN_TRANS_PROC_NSEC: %d\n", SO_MIN_TRANS_PROC_NSEC);
    printf("SO_MAX_TRANS_PROC_NSEC: %d\n", SO_MAX_TRANS_PROC_NSEC);
    printf("#SO_REGISTRY_SIZE: %d\n", SO_REGISTRY_SIZE);
    printf("SO_BUDGET_INIT: %d\n", SO_BUDGET_INIT);
    printf("SO_SIM_SEC: %d\n", SO_SIM_SEC);
    printf("SO_FRIENDS_NUM: %d\n", SO_FRIENDS_NUM);
    printf("SO_HOPS: %d\n\n", SO_HOPS);

    /* if ((private_key = ftok("./utils/private-key", 'a')) == -1)
        error("ftok() failed!");

    printf("%d\n", private_key);

    if ((msgId = msgget(private_key, IPC_CREAT | 0666)) == -1)
        error("msgget() failed!");

    printf("%d\n", msgId); */

    for (i = 0; i < SO_USERS_NUM; ++i) {
        system("./utente");
    }
}