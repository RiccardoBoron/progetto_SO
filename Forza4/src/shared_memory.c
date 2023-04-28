#include <sys/shm.h>
#include <sys/stat.h>
#include<sys/types.h>
#include<unistd.h>

#include "errExit.h"
#include "shared_memory.h"

//Implementazione delle funzioni dichiarate nella libreria shared_memory.h

int alloc_shared_memory(key_t shmKey, size_t size) {
    // Acquisisco o creo un segmento di memoria condivisa
    int shmid = shmget(shmKey, size, IPC_CREAT | S_IRUSR | S_IWUSR);
    if(shmid == -1)
        errExit("shmget failed");

    return shmid;
}

void *get_shared_memory(int shmid, int shmflg) {
    //Attacco la memoria condivisa ad un identificativo per poi poterci leggere
    void *ptr_sh = shmat(shmid, NULL, shmflg);
    if(ptr_sh == (void *)-1)
        errExit("shmat failed");

    return ptr_sh;
}

void free_shared_memory(void *ptr_sh) {
    //Stacco il segmento d memoria condivisa ovvero lo svuoto
    if(shmdt(ptr_sh) == -1)
        errExit("shmdt failed");
}

void remove_shared_memory(int shmid) {
    //Elimino il segmento di memoria condivisa
    if(shmctl(shmid, IPC_RMID, NULL) == -1)
        errExit("shmctl failed");
}