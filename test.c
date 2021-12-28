#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <string.h>

#include "master.h"

struct sigaction act;

int queueId;

void hdl(int sig, siginfo_t* siginfo, void* context) {
    /**
	 * 	Questo è l'handler dei segnali, gestisce i segnali:
	 *  	-	SIGINT
	 *  	-	SIGTERM
	 *  	-	SIGSEGV
     *      -   SIGUSR1
	 **/
    switch (sig) {
        case SIGINT: {
            pid_t pid = (*siginfo).si_pid;
            printf("\n[ %sSIGINT%s ] Impossible to send transaction to %d\n", YELLOW, WHITE, pid);
            kill(pid, SIGINT);
            exit(EXIT_SUCCESS);
        }

        case SIGTERM:
            exit(EXIT_FAILURE);

        case SIGSEGV:
            error("SEGMENTATION VIOLATION [USER]");

        case SIGUSR1: {
            printf("\n[ %sSIGUSR1%s ] Transaction creation due to SIGUSR1\n", YELLOW, WHITE);
            break;
        }

        case SIGUSR2: {
            printf("\n[ %sSIGUSR2%s ] Impossible to send transaction to %d\n", YELLOW, WHITE, siginfo->si_pid);
            break;
        }
    }
}

int main() {
    /* Copia il carattere \0 in act per tutta la sua lunghezza */
    memset(&act, '\0', sizeof(act));

    /* Dico a sigaction (act) quale handler deve mandare in esecuzione */
    act.sa_sigaction = &hdl;

    act.sa_flags = SA_SIGINFO | SA_NODEFER;

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
    if (sigaction(SIGUSR2, &act, NULL) < 0) {
        perror(RED "[USER] Sigaction: Failed to assign SIGUSR2 to custom handler" WHITE);
        exit(EXIT_FAILURE);
    }

    message msg;
    int i = 0;
    int SO_RETRY = 2;

    /* »»»»»»»»»» CODA DI MESSAGGI »»»»»»»»»» */
    /* in base al nodo estratto (nodeReceiver) scelgo la coda di messaggi di quel nodo */
    if ((queueId = msgget(ftok("./utils/private-key", 'q'), IPC_EXCL | 0644)) < 0) {
        perror(RED "[USER] Message Queue creation failure" WHITE);
        exit(EXIT_FAILURE);
    }

    msg.mtype = getpid();
    msg.transaction.quantity = 10;
    msg.transaction.receiver = -1;
    msg.transaction.reward = 5;
    msg.transaction.sender = getpid();
    msg.transaction.timestamp = 123346298;
    printf("%d Inviando...\n", getpid());
    if (msgsnd(queueId, &msg, sizeof(msg) - sizeof(pid_t), IPC_NOWAIT) < 0) {
        perror("");
        exit(EXIT_FAILURE);
    }
    printf("%d Inviato!\n", getpid());

    while (1) {
        fflush(stdout);
        printf("#");
        sleep(1);
    }

    exit(EXIT_SUCCESS);
}