#ifndef _SEM_H
#define _SEM_H

#include <sys/sem.h> /*  per i semafori. */

/** Enumerazione per la gestione dei segnali
 *      - userSync: semNum = 0
 *      - nodeSync: semNum = 1
 *      - userShm: semNum = 2
 *      - nodeShm: semNum = 3
 *      - ledgerShm: semNum = 4
 *      - print: semNum = 5
 **/
enum Sem { userSync,
           nodeSync,
           userShm,
           nodeShm,
           ledgerShm,
           print };

union semun {
    int val;               /* value for SETVAL */
    struct semid_ds* buf;  /* buffer for IPC_STAT, IPC_SET */
    unsigned short* array; /* array for GETALL, SETALL */
/* Linux specific part */
#if defined(__linux__)
    struct seminfo* __buf; /* buffer for IPC_INFO */
#endif
};

/* Inizializza il semaforo a 1 (DISPONIBILE) */
int initSemAvailable(int semId, int semNum);

/* Inizializza il semaforo a 0 (IN USO) */
int initSemInUse(int semId, int semNum);

/* Riserva il semaforo - decrementa di 1 */
int reserveSem(int semId, int semNum);

/* Rilascia il semaforo - incrementa di 1 */
int releaseSem(int semId, int semNum);

/* Rimuove il semaforo (IPC_RMID) */
int removeSem(int semId, int semNum);

#endif