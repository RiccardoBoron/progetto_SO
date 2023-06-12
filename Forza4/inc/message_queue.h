#ifndef _MESSAGE_QUEUE_HH
#define _MESSAGE_QUEUE_HH


struct myMsg {
    int mtype;
    int posRiga;
    int posColonna;
    int aUtoGame;
    pid_t pidClient;
};

void msgRcv(int msqid, struct myMsg *myMsg, ssize_t siz, int param, int mtype);
void msgSnd(int msqid, struct myMsg *myMsg, ssize_t siz, int mtype);

#endif