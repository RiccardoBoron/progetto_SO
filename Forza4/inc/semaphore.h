#ifndef _SEMAPHORE_HH
#define _SEMAPHORE_HH

// Definisco la union di un semaforo
union senum { //Non posso chiamarlo semun altrimenti ho un errore
    int val;
    struct semid_ds * buf;
    unsigned short * array;
};

/*
    Dichiaro una funzione di supporto per manipolare i valori dei semafori
    che fanno parte del set di semafori, semid é un identificatore del set di semafori,
    sem_num é il numero del semaforo nel set, sem_op é l'operazione del semaforo
*/

void semOp(int semid, unsigned short sem_num, short sem_op);

#endif