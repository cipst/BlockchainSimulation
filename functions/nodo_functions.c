#include "../header.h"

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
            releaseSem(semId, nodeShm);
            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            exit(EXIT_SUCCESS);

        case SIGTERM:
            releaseSem(semId, nodeShm);
            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            exit(EXIT_FAILURE);

        case SIGSEGV:
            releaseSem(semId, nodeShm);
            shmdt(mastro);
            shmdt(users);
            shmdt(nodes);
            error("SEGMENTATION VIOLATION [NODE]");

        case SIGUSR1: {
            /* transaction trans;*/
            reserveSem(semId, print);
            printf("\n[ %sSIGUSR1%s ] Transaction creation due to SIGUSR1\n", YELLOW, RESET);
            releaseSem(semId, print);
            /*trans = createTransaction();
            sendTransaction(trans); */
            break;
        }
    }
}

int addTransaction(transaction trans, transaction* pool, int pos) {
    *(pool + pos) = trans; /* salvo la transazione nella transaction pool */

    pos++; /* posizione per la prossima transazione && grandezza della transaction pool*/

    reserveSem(semId, nodeShm);
    (nodes + offset)->poolSize = pos; /* aggiorno l'attuale grandezza della transaction pool in memoria condivisa */
    releaseSem(semId, nodeShm);

    return pos;
}

void printPool(transaction* pool, int pos) {
    int j;
    reserveSem(semId, print);
    printf("\n[ %s%d%s ] Transaction pool\n", BLUE, getpid(), RESET);
    for (j = 0; j < pos; j++) {
        printTransaction((pool + j));
    }
    releaseSem(semId, print);
}