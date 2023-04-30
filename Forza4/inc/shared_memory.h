#ifndef _SHARED_MEMORY_HH
#define _SHARED_MEMORY_HH

#include <stdlib.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include<sys/types.h>
#include<unistd.h>

// Questa truttura di richiesta rappresenta una richiesta di invio da parte del client
struct Request {
    char matrix[50][50];
    int rows;
    int colums;
    char Gettone1;
    char Gettone2;
    int vincitore;
};

/*
    Funzione di allocazione della memoria condivisa, nel cas non esistesse,
    ritorna il shmid nel caso di successo altrimenti termina il processo chiamante
*/
int alloc_shared_memory(key_t shmKey, size_t size);

/*
    Questa funzione attacca un segmento di memoria condivisa nello spazio logico di indirizzi
    del processo chimante.
    Ritorna il puntatore al segmento di memoria condivisa attaccato in caso di successo, 
    altrimenti termina il processo chiamante.
    Serve per prelevare dati dalla memoria condivisa.
*/
void *get_shared_memory(int shmid, int shmflg);

/*
    Questa funzione stacca un segmento di memoria condivisa dallo spazio logico degli indirizzi 
    del processo chiamante.
    Se no ha successo termina il processo chiamante
    Serve per svuotare la memoria condivisa.
*/
void free_shared_memory(void *ptr_sh);

/*
    Questa funzione rimuove un segmento di memoria condivisa,
    nel caso non avesse successo termina il processo chiamante
*/
void remove_shared_memory(int shmid);

#endif