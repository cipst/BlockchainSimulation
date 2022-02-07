#ifndef _UTENTE_H
#define _UTENTE_H

#include "header.h"

/** Crea una nuova transazione
 * 
 * @return la transazione da inviare al nodo
 */
transaction createTransaction();

/** Invia la transazione a un nodo
 * 
 *  @param trans transazione da inviare
 */
void sendTransaction(transaction*);

/** Controlla se nella coda di messaggi RESPONSE è presente un messaggio per l'utente
 *  - se è presente un messaggio
 *  - - l'invio della precedente transazione è fallito ⇒ viene incrementato il contatore TRY (n° volte consecutive che l'utente fallisce)
 *  - - se TRY ha raggiunto SO_RETRY (n° volte consecutive MAX che l'utente può fallire) ⇒ l'utente termina
 * 
 *  - se non è presente un messaggio
 *  - - TRY viene resettato a 0 ⇒ l'utente è riuscito ad inviare la transazione
 */
void receiveResponse();

/** Controlla se un utente è ancora vivo
 *  
 *  @param offset   posizione dell'utente in 'users' da controllare
 * 
 *  @return 0 se è ancora vivo, -1 altrimenti
 */
int isAlive(int);

/* Inizializza tutte le struture IPC necessarie al processo utente */
void initUserIPC();

#endif