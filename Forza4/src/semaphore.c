#include <sys/sem.h>
#include <errno.h>
#include "semaphore.h"
#include "errExit.h"

void semOp(int semid, unsigned short sem_num, short sem_op) {
    struct sembuf sop = {
        .sem_num = sem_num,
        .sem_op = sem_op,
        .sem_flg = 0
    };

    while(semop(semid, &sop, 1) == -1){
        if(errno != EINTR){
            errExit("semop failed");
        }else{
           //continue
        }
    }
}