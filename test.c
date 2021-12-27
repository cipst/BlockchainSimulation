#include "master.h"

int queueId;

int main() {
    message msg;
    int i = 0;
    int SO_RETRY = 2;

    /* »»»»»»»»»» CODA DI MESSAGGI »»»»»»»»»» */
    /* in base al nodo estratto (nodeReceiver) scelgo la coda di messaggi di quel nodo */
    if ((queueId = msgget(ftok("./utils/private-key", 'q'), IPC_EXCL | 0644)) < 0) {
        perror(RED "[USER] Message Queue creation failure" WHITE);
        exit(EXIT_FAILURE);
    }

    msg.node = getpid();
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

    exit(EXIT_SUCCESS);
}