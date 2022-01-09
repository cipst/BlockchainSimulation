#ifndef _MASTER_H
#define _MASTER_H

#include "header.h"

/** Funzione che uccide tutti i processi in shared memory (sia 'users' sia 'nodes').
*
*   @param sig segnale da inviare a tutti i processi presenti nella shared memory;
* */
void killAll(int);

/* Inizializza le variabili di configurazione e controlla che i valori siano corretti */
void readConfigFile();

/** Stampa tutte le informazioni ritenute utili prima della terminazione del programma
 * 
 *   Vengono stampate a video:
 *   -   Bilancio di ogni processo utente (compresi quelli terminati prematuramente)
 *   -   Bilancio di ogni processo nodo
 *   -   Numero di processi utente terminati prematuramente
 *   -   Numero di blocchi nel libro mastro
 *   -   Per ogni processo nodo, numero di transazioni ancora presenti nella transaction pool
 **/
void printStats();

/** Stampa a video gli ID degli oggetti IPC:
*    -  semId: l'id del semaforo
*    -  shmLedgerId: l'id dell'area di shared memory dedicata al libro mastro (creata con shmget)
*    -  shmUsersId: l'id dell'area di shared memory dedicata ai processi Utente (creata con shmget)
*    -  shmNodesId: l'id dell'area di shared memory dedicata ai processi Nodo (creata con shmget)
*    -  shmActiveUsersId: l'id dell'area di shared memory usata dal gestore per sincronizzare l'avvio di tutti i processi
**/
void printIpcStatus();

/* Scrive su stdout il valore delle variabili di configurazione */
void printConfigVal();

#endif