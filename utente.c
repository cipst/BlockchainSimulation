#include "master.h"

/* Calcola il bilancio delle transazioni presenti nel Libro Mastro */
int balanceFromLedger(ledger*);

int main(int argc, char** argv) {    
    int balance = 0;
    int ledgerId;
    ledger* mastro;
    int i, j;
    int semId;

    if ((semId = semget(ftok("../utils/private-key", 's'), 1, 0644)) < 0)
        error("Semaphore get. (semget)");

    if (reserveSem(semId, 0) < 0)
        error("Semaphore reserve. (reserveSem)");

    if ((ledgerId = shmget(ftok("../utils/private-key", 'l'), sizeof(ledger), 0644)) < 0)
        error("Ledger ID. (shmget)");

    mastro = (ledger*)shmat(ledgerId, NULL, 0);
    if (mastro == (void*)-1)
        error("Ledger attaching. (shmat)");

    balance = balanceFromLedger(mastro);
    printf("[%d] BILANCIO: %d\n", getpid(), balance);

    if (shmdt(mastro) < 0)
        error("Ledger detaching. (shmdt)");

    printf("\t%d Termino...\n", getpid());

    exit(EXIT_SUCCESS);
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