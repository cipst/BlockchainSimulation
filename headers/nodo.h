#ifndef _NODO_H
#define _NODO_H

#include "header.h"

transaction* pool; /* transaction pool che conterrà tutte le transazioni di un nodo */

/** Aggiunge una transazione alla transaction pool
 * 
 * @param pos   riferimento alla posizione corrente nella transaction pool
 * @param trans transazione da aggiungere alla transaction pool
 * 
 * @return 0 se la transazione è stata aggiunta, -1 altrimenti (transaction pool piena)
 **/
int addTransaction(unsigned int* pos, transaction trans);

/** Aggiunge una transazione alla transaction pool
 * 
 * @param pos   posizione della transazione da eliminare dalla transaction pool
 * 
 * @return 0 se la transazione è stata eliminata, -1 altrimenti
 **/
void removeTransaction(unsigned int pos);

/** Crea un blocco candidato da inviare al libro mastro
 * 
 * @param b     riferimento al blocco candidato
 * @param pos   riferimento alla posizione corrente nella transaction pool
 * 
 * @return 0 se il blocco è stato creato, -1 altrimenti
 **/
int createBlock(block*, unsigned int*);

/** Rimuove le transazioni presenti in un blocco dalla transaction pool
 * @param b riferimento al blocco candidato
 * 
 * @return 0 se tutte le transazioni sono state eliminate, -1 altrimenti
 */
int removeBlockFromPool(block*);

/** Aggiorna il Libro Mastro aggiungendo il blocco creato
 * 
 *  @param b    blocco candidato che entra a far parte del Libro 
 * 
 *  @return 0 se il blocco è stato aggiunto, -1 altrimenti
 */
int updateLedger(block*);

/* Stampa a video la transaction pool */
void printPool();

#endif