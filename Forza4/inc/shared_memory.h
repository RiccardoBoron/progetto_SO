#ifndef _SHARED_MEMORY_HH
#define _SHARED_MEMORY_HH

#include <stdlib.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include<sys/types.h>
#include<unistd.h>


struct Shared {
    int rows;
    int colums;
    char Gettone1;
    char Gettone2;
    int gettoneInizio;
    int vincitore;
    int autoGameFlag;
    pid_t pidServer;
};

int alloc_shared_memory(key_t shmKey, size_t size);

void *get_shared_memory(int shmid, int shmflg);

void free_shared_memory(void *ptr_sh);

void remove_shared_memory(int shmid);

#endif