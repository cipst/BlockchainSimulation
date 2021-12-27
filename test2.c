#include "master.h"

transaction* pool;
int queueId;

int main() {
    message msg;
    int SO_TP_SIZE = 5;
    pool = (transaction*)malloc(SO_TP_SIZE); /* transaction pool che conterrà tutte le transazioni di questo nodo */

    printf("%d\n", getpid());

    /* »»»»»»»»»» CODA DI MESSAGGI »»»»»»»»»» */
    if ((queueId = msgget(ftok("./utils/private-key", 'q'), IPC_CREAT | 0400 | 0200 | 040 | 020)) < 0) {
        perror(RED "[NODE] Message Queue creation failure" WHITE);
        exit(EXIT_FAILURE);
    }

    msgrcv(queueId, &msg, sizeof(msg) - sizeof(pid_t), 0, MSG_NOERROR);
    printf("[%d] PID LETTO: %d\n", getpid(), msg.node);

    printTransaction(msg.transaction);

    free(pool);
    exit(EXIT_SUCCESS);
}