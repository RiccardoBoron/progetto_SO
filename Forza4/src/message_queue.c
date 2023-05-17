#include <stdlib.h>
#include <sys/msg.h>
#include <errno.h>

#include "message_queue.h"
#include "errExit.h"

void msgRcv(int msqid, struct myMsg *myMsg, ssize_t siz, int param, int mtype){
    while(msgrcv(msqid, myMsg, siz, 0, 0) == -1){
        if(errno != EINTR){
            errExit("msgrcv failed\n");
        }else{
            //continue
        }      
    }
}

void msgSnd(int msqid, struct myMsg *myMsg, ssize_t siz, int mtype){
     while(msgsnd(msqid, myMsg, siz, 0) == -1){
        if(errno != EINTR){
            errExit("msgrcv failed\n");
        }else{
            //continue
        }      
    }
}
