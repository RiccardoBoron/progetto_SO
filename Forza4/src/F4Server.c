#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <signal.h>
#include <sys/wait.h>
#include "errExit.h"
#include "semaphore.h"
#include "shared_memory.h"
#include "message_queue.h"


//------------------------- PROTOTIPI DI FUNZIONI -------------------------//
void azzera(char *m, int, int);
int controlloVittoria(char *m, int rows, int columns, char Gettone1, char Gettone2, int posColonna);
void sigHandler(int sig);
void sigTimer(int sig);
void quitCtrlClient(int sig);
void terminalSignal(int sig);
void remove_msq();

//Variabili globali
int contaSegnale = 0;
int semid = 0;
int shmid = 0;
struct Shared *shared;
int shmId = 0;
int *sharedMemory;
pid_t pidClient1 = 0;
pid_t pidClient2 = 0;
int msqid = -1; 

//------------------------- FUNZIONE PER LA CREAZIONE DI UN SET DI SEMAFORI -------------------------//
int create_sem_set(key_t semkey) {
    //Creazione di un set di 4 semafori
    int semid = semget(semkey, 4, IPC_CREAT | S_IRUSR | S_IWUSR);
    if(semid == -1)
        errExit("semget failed");

    //Inizializzazione del set di semafori
    union senum arg;
    unsigned short values[] = {0, 0, 1, 0};
    arg.array = values;

    if(semctl(semid, 0, SETALL, arg) == -1)
        errExit("semctl SETALL failed");

    return semid;
}

int main(int argc, char *argv[]) {

    //Key
    key_t shmKey = 12; //Memoria condivisa
    key_t semKey = 23; //Semafori
    key_t msgKey = 10; //Coda dei messaggi
    key_t shmKey2 = 34; //Memoria condivisa usata come matrice

    //Set di segnali
    sigset_t mySet;
    //Iniziallizzazio di mySet con tutti i segnali
    sigfillset(&mySet);
    //Rimozione del segnale SIGINT, SIGALARM, SIGUSR2, SIGHUP da mySet
    sigdelset(&mySet, SIGINT);
    sigdelset(&mySet, SIGALRM);
    sigdelset(&mySet, SIGUSR2);
    sigdelset(&mySet, SIGHUP);
    //blocco tutti gli altri segnali su mySet
    sigprocmask(SIG_SETMASK, &mySet, NULL);


    //Controllo input inseriti da linea di comando
    if(argc != 5) {
        printf("\nErrore input: devi inserire ./server dim_righe dim_colonne gettone1 gettone2\n");
        exit(1);
    }

    //Controllo sulle righe e colonne della matrice (area di gioco)
    int rows = atoi(argv[1]);
    int colums = atoi(argv[2]);
    if(rows < 5 || colums < 5) {
        printf("\nErrore, il campo di gioco deve essere almeno 5x5\n");
        exit(1);
    }

    //Acquisizione dei due gettoni di gioco 
    char Gettone1 = *argv[3];
    char Gettone2 = *argv[4];

    //Controllo dei gettoni
    if(Gettone1 == Gettone2){
        printf("\nErrore Gettoni: gettoni non possono essere uguali\n");
        exit(1);
    }

    //------------------------- CREAZIONE MEMEORIA CONDIVISA -------------------------//
    
    //Memoria condivisa dove salvo la dimensione delle righe e delle colonne e una variabile per determinare il vincitore
    shmid = alloc_shared_memory(shmKey, sizeof(struct Shared));
    shared = (struct Shared*)get_shared_memory(shmid, 0);

    //Acquisiszione pid del server
    shared->pidServer = getpid();

    //Controllo il numero di server, solo un server puó essere attivo
    shared ->nServer ++;
    if(shared->nServer > 1){
        errExit("\nEsiste giá un server attivo...\n");
    }

    //Memoria condivisa che viene utilizzata come una matrice
    shmId = shmget(shmKey2, rows * colums * sizeof(int), IPC_CREAT | S_IRUSR | S_IWUSR);
    if(shmId == -1){
        errExit("shmget failed");
    }

    sharedMemory = (int*)shmat(shmId, NULL, 0);
    if(sharedMemory == (void *)-1){
        errExit("shmat failed");
    }
    
    char(*matrix)[colums] = (char(*)[colums])sharedMemory;


    //------------------------- CREAZIONE SET DI SEMAFORI -------------------------//
    semid = create_sem_set(semKey);

    //------------------------- CREAZIONE CODA DEI MESSAGGI -------------------------//
    msqid = msgget(msgKey, IPC_CREAT | S_IRUSR | S_IWUSR);
    if(msqid == -1){
        errExit("msgget failed\n");
    }

    struct myMsg mossa;
    ssize_t siz = sizeof(struct myMsg) - sizeof(long);
    mossa.mtype = 1;
    mossa.posRiga = 0;
    mossa.posColonna = 0;
    mossa.aUtoGame = 0;
    

    //------------------------ GESTIONE DEI SEGNALI ------------------------//
    if(signal(SIGINT, sigHandler) == SIG_ERR)
        errExit("change signal handler failed");

    if(signal(SIGALRM, sigTimer) == SIG_ERR)
        errExit("change signal handler failed");


    if(signal(SIGUSR2, quitCtrlClient) == SIG_ERR)
        errExit("change signal handler failed");

    if(signal(SIGHUP, terminalSignal) == SIG_ERR)
        errExit("change signal handler failed");

    //------------------------- IL SERVER ASPETTA I CLIENT -------------------------//

    printf("\n<Server> Aspetto i client...\n");
    semOp(semid, 0, -1); //Attesa primo client
    printf("\n<Server> Client 1 arrivato, attendo Client 2\n");

    //Acquisisco il pid del primo client
    msgRcv(msqid, &mossa, siz, 0, 0);
    pidClient1 = mossa.pidClient;

    //Aspetto che i client mi dicano se devo giocare in modalitá automaticGame
    int autoGame = shared->autoGameFlag;
    
    //Se gioco in autogame il server si duplica ed esegue la parte di client relativa all'autogiocatore
    if(autoGame == 1){
        pid_t pid = fork();
        if(pid == -1){
            errExit("\nFork failed\n");
        }else if(pid == 0){
            char *args[] = {"giocatore", "1", NULL};
            //Faccio la exec()
            printf("\n<Server> Duplicazione server ed esecuzione client automatico...\n");
            execv("./F4Client", args);
            errExit("execv failed");
        }
    }

    //Attesa secondo client
    semOp(semid, 0, -1); 
    printf("\n<Server> Client 2 arrivato\n");

    //Acquisisco il pid del secondo client
    msgRcv(msqid, &mossa, siz, 0, 0);
    pidClient2 = mossa.pidClient;
    

    //------------------------- INIZIALIZZAZIONE CAMPO DI GIOCO -------------------------//
    printf("\n<Server> Preparo il campo di gioco...\n");

    //Inserimento delle righe e colonne nella memoria condivisa
    shared->rows = rows;
    shared->colums = colums;
    shared->Gettone1 = Gettone1;
    shared->Gettone2 = Gettone2;
    shared->vincitore = 0;
    shared->FlagVittoriaAutogame = 0;

    //Inizializzazione della matrice (tutta vuota all'inizio)
    azzera(&matrix[0][0], rows, colums);

    printf("\n<Server> Campo di gioco pronto, gioco in corso...\n");

    //Il server fa partire i client
    semOp(semid, 1, 1);
    semOp(semid, 1, 1);
    
    //------------------------- GESTIONE DEL GIOCO -------------------------//
    int nt = 0, turn = 1, fine = 1;

    while(fine == 1){
        //Ricevo la mossa dal client 
        msgRcv(msqid, &mossa, siz, 0, 0);
        if(shared->vincitore == 3){
            break;
        }else{
            nt ++; //Incremento il numero di mosse totali
            if(turn == 1){
                turn ++;
                matrix[mossa.posRiga][mossa.posColonna] = Gettone1; //Inserimento gettone nella matrice
            }else {
                turn --;
                matrix[mossa.posRiga][mossa.posColonna] = Gettone2; //Inserimento gettone nella matrice
            }

            //CONTROLLO VITTORIA
            if(controlloVittoria(&matrix[0][0], shared->rows, shared->colums, Gettone1, Gettone2, mossa.posColonna) == 1){
                shared->vincitore = 1;
                fine = 0;
            }
            
            //CONTROLLO PAREGGIO
            if(nt == shared->rows * shared->colums && shared->vincitore == 0){
                shared->vincitore = 2;
                fine = 0;
            }
        }
        //Avviso il client che puó stampare
        if(autoGame == 0){
            semOp(semid, 1, 1);
        }
        
        if(autoGame == 1 && mossa.aUtoGame == 0){
            if(shared->vincitore == 1){
                shared->FlagVittoriaAutogame = 1; //Vince Autogame
                semOp(semid, 3, 1);
            }
            semOp(semid, 2, 1);
        }

        if(autoGame == 1 && mossa.aUtoGame == 1){
            if(shared->vincitore == 1)
                shared->FlagVittoriaAutogame = 2; //Vince Client Normale
            semOp(semid, 3, 1);
        }

        if(autoGame == 1){
            semOp(semid, 1, 1);
        }

        if(autoGame == 0){
            semOp(semid, 2, 1);
        }
    } 
    
    //Aspetto la terminazione dei client per eliminare semafori e shared memeory
    printf("<Server> Aspetto che i Client terminino...\n"); 
    semOp(semid, 0, -1); 
    printf("<Server> Client 1 terminato, aspetto per Client 2 \n"); 
    semOp(semid, 0, -1);
    printf("<Server> Client 2 terminato, FINE GIOCO\n");

    
    //------------------------- ELIMINAZIONE SEMAFORI, SHARED MEMORY, MESSAGE QUEUE -------------------------//
    remove_msq();

    return 0;
}

//Funzione di inizializza il campo da gioco
void azzera(char *m, int r, int c){
    int i,j;
    for(i=0; i<r; i++)
        for(j=0; j<c; j++)
          *(m + i * c + j) = ' ';
}

//Funzione di controllo vittoria
int controlloVittoria(char *m,int rows, int columns, char Gettone1, char Gettone2, int posColonna){
    //Variabili per verificare la vittoria
    int verticale1 = 0, verticale2 = 0;
    int orizzontale1 = 0, orizzontale2 = 0;

    //Controllo verticale
    for(int i = 0; i < rows; i++){
        if( *(m + i * columns + posColonna) == Gettone1 && *(m + (i + 1) * columns + posColonna) == Gettone1){
            verticale1++;
        }
        if( *(m + i * columns + posColonna) == Gettone2 && *(m + (i + 1) * columns + posColonna) == Gettone2){
            verticale2++;
        }
        if( (*(m + i * columns + posColonna) == Gettone1 && *(m + (i + 1) * columns + posColonna) == Gettone2) || (*(m + i * columns + posColonna) == Gettone2 && *(m + (i + 1) * columns + posColonna) == Gettone1)){
            verticale2 = 0;
            verticale1 = 0;
        }
        if(verticale1 == 3 || verticale2 == 3)
            return 1;
    }
    
    //Controllo orizzontale
    for(int i = 0; i < rows; i++){
        for(int j = 0; j < columns; j++){
            if( *(m + i * columns + j) == Gettone1 && *(m + i * columns + j + 1) == Gettone1){
                orizzontale1++;
            }
            if( *(m + i * columns + j) == Gettone2 && *(m + i * columns + j + 1) == Gettone2){
                orizzontale2++;
            }
            
            if(orizzontale1 == 3 || orizzontale2 == 3)
                return 1;

            if( (*(m + i * columns + j) == Gettone1 && *(m + i * columns + j + 1) != Gettone1) ||  (*(m + i * columns + j) == Gettone2 && *(m + i * columns + j + 1) != Gettone2)){
                orizzontale1 = 0;
                orizzontale2 = 0;
            }
        }
        if(orizzontale1 == 3 || orizzontale2 == 3)
            return 1;
        orizzontale1 = 0;
        orizzontale2 = 0;
    }

    //Controllo diagonali principali
    for (int i = 0; i < rows - 3; i++) {
        for (int j = 0; j < columns - 3; j++) {
            if (*(m + i * columns + j) == Gettone1 && *(m + (i + 1) * columns + j + 1) == Gettone1 && *(m + (i + 2) * columns + j + 2) == Gettone1 && *(m + (i + 3) * columns + j + 3) == Gettone1) {
                return 1;
            }
            if (*(m + i * columns + j) == Gettone2 && *(m + (i + 1) * columns + j + 1) == Gettone2 && *(m + (i + 2) * columns + j + 2) == Gettone2 && *(m + (i + 3) * columns + j + 3) == Gettone2) {
                return 1;
            }
        }
    }

    //Controllo diagonali secondarie
    for (int i = 0; i < rows - 3; i++) {
        for (int j = columns - 1; j >= 3; j--) {
            if (*(m + i * columns + j) == Gettone1 && *(m + (i + 1) * columns + j - 1) == Gettone1 && *(m + (i + 2) * columns + j - 2) == Gettone1 && *(m + (i + 3) * columns + j - 3) == Gettone1) {
                return 1;
            }
            if (*(m + i * columns + j) == Gettone2 && *(m + (i + 1) * columns + j - 1) == Gettone2 && *(m + (i + 2) * columns + j - 2) == Gettone2 && *(m + (i + 3) * columns + j - 3) == Gettone2) {
                return 1;
            }
        }
    }

    return 0;
}

//Funzion di gestione del segnale Ctrl+C
void sigHandler(int sig){
    contaSegnale ++;
    if(contaSegnale == 1) {
        printf("Sicuro di voler terminare?\n");
        alarm(5);
    }
    if(contaSegnale == 2){
        shared->vincitore = -1;
        if(kill(pidClient1, SIGINT) == -1){
            errExit("kill failed");
        }

        if(kill(pidClient2, SIGINT)){
            errExit("kill failed");
        }

        remove_msq();
        exit(0);
    }
}

//Ripristina il gioco se entro 5 secondi non si ripreme Ctrl+C (SIGALARM)
void sigTimer(int sig){
    printf("Riprendo il gioco\n");
    contaSegnale = 0;
}

//Nel caso il client termina con Ctrl+C
void quitCtrlClient(int sig){
    printf("\nPartita terminata da un Client\n");
    if(pidClient1 != shared->pidServer){
        if(kill(pidClient1, SIGUSR1) == -1){
            errExit("kill failed");
        }
    }else if(pidClient2 != shared->pidServer){
        if(kill(pidClient2, SIGUSR1) == -1){
            errExit("kill failed");
        }
    }

    remove_msq();
    exit(0);
}

//Gestione del segnale di chiusura del terminale
void terminalSignal(int sig){
     shared->vincitore = -1;
    if(kill(pidClient1, SIGINT) == -1){
        errExit("kill failed");
    }

    if(kill(pidClient2, SIGINT)){
        errExit("kill failed");
    }

    remove_msq();
    exit(0);
}

//FUNZIONE DI ELIMINAZIONE SEMAFORI, SHARED MEMORY, MESSAGE QUEUE 
void remove_msq(){
    if(semctl(semid, 0, IPC_RMID, NULL) == -1)
        errExit("smctl failed\n");

    free_shared_memory(shared);

    remove_shared_memory(shmid);

    if(shmdt(sharedMemory) == -1)
        errExit("shmdt failed\n"); 

    if(shmctl(shmId, IPC_RMID, NULL) == -1)
        errExit("shmctl failed");

    if(msgctl(msqid, IPC_RMID, NULL) == -1)
        errExit("msgctl IPC_RMID failed");
}


/************************************** 
*Matricola: VR471376
*Nome e cognome: Riccardo Boron

*Matricola: VR479274
*Nome e cognome: Mattia Riva

*Matricola: VR478582
*Nome e cognome: Alessia Foglieni

*Data di realizzazione: 25/06/2023
*************************************/

