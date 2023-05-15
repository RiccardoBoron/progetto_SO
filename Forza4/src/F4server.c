#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <signal.h>
#include "errExit.h"
#include "semaphore.h"
#include "shared_memory.h"


//------------------------- PROTOTIPI DI FUNZIONI -------------------------//
void azzera(char *m, int, int);
int controlloVittoria(char *m, int rows, int columns, char Gettone1, char Gettone2, int posColonna);
void sigHandler(int sig);

//Variabili globali
int contaSegnale = 0;
int semid = 0;
int shmid = 0;
struct Shared *shared;


//------------------------- FUNZIONE PER LA CREAZIONE DI UN SET DI SEMAFORI -------------------------//
int create_sem_set(key_t semkey) {
    //Creazione di un set di 4 semafori
    int semid = semget(semkey, 4, IPC_CREAT | S_IRUSR | S_IWUSR);
    if(semid == -1)
        errExit("semget failed");

    //Inizializzazione del set di semafori
    union semun arg;
    unsigned short values[] = {0, 0, 1, 0};
    arg.array = values;

    if(semctl(semid, 0, SETALL, arg) == -1)
        errExit("semctl SETALL failed");

    return semid;
}

//Inizializzazione msqid
int msqid = -1; 

int main(int argc, char *argv[]) {

    //Key
    key_t shmKey = 12; //Memoria condivisa
    key_t semKey = 23; //Semafori
    key_t msgKey = 10; //Coda dei messaggi
    key_t shmKey2 = 34; //Memoria condivisa usata come matrice

    //set di segnali
    sigset_t mySet;
    
    //Iniziallizzazio di mySet con tutti i segnali
    sigfillset(&mySet);
    //Rimozione del segnale SIGINT da mySet
    sigdelset(&mySet, SIGINT);
    // blocking all signals but SIGINT
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


    //------------------------- CREAZIONE MEMEORIA CONDIVISA -------------------------//
    
    //memoria condivisa dove salvo la dimensione delle righe e delle colonne e una variabile per determinare il vincitore
    shmid = alloc_shared_memory(shmKey, sizeof(struct Shared));
    shared = (struct Shared*)get_shared_memory(shmid, 0);

   //Memoria condivisa che viene utilizzata come una matrice
    int shmId = shmget(shmKey2, rows * colums * sizeof(int), IPC_CREAT | S_IRUSR | S_IWUSR);
    int *sharedMemory = (int*)shmat(shmId, NULL, 0);
    char(*matrix)[colums] = (char(*)[colums])sharedMemory;


    //------------------------- CREAZIONE SET DI SEMAFORI -------------------------//
    semid = create_sem_set(semKey);

    //------------------------- CREAZIONE CODA DEI MESSAGGI -------------------------//
    int msqid = msgget(msgKey, IPC_CREAT | S_IRUSR | S_IWUSR);
    if(msqid == -1){
        errExit("msgget failed\n");
    }

    //Struttura condivisa con coda dei messaggi, viene utilizzata per inserire il gettone nella posizione giusta
    struct myMsg{
        int mtype;
        int posRiga;
        int posColonna;
    } mossa;

    ssize_t siz = sizeof(struct myMsg) - sizeof(long);
    mossa.mtype = 1;
    mossa.posRiga = 0;
    mossa.posColonna = 0;
    
    //------------------------ GESTIONE DEI SEGNALI ------------------------//
    if(signal(SIGINT, sigHandler) == SIG_ERR)
        errExit("change signal handler failed");


    //------------------------- IL SERVER ASPETTA I CLIENT -------------------------//
    printf("\n<Servre> Aspetto i client...\n");
    semOp(semid, 0, -1); //Attesa primo client
    printf("\n<Server> Client 1 arrivato, attendo Client 2\n");

    //Aspetto che i client mi dicano se devo giocare in modalitá automaticGame
    int autoGame = shared->vincitore;
    shared->vincitore = 0;

    //Se gioco in autogame il server si duplica ed esegue la parte di client relativa all'autogiocatore
    if(autoGame == 1){
        pid_t pid = fork();
        if(pid == -1){
            errExit("\nFork failed\n");
        }else if(pid == 0){
            char *args[] = {"giocatore", "1", NULL};
            //Faccio la exec()
            printf("\nDuplicazione server\n");
            if(execv("./client", args) == -1){
                errExit("execv");
            }
            while(shared->vincitore == 0);
            printf("Figlio terminato");
            exit(0);
            
        }
    }

    //Attesa secondo client
    semOp(semid, 0, -1); //Attesa secondo client
    printf("\n<Server> Client 2 arrivato\n");
    
    //------------------------- INIZIALIZZAZIONE CAMPO DI GIOCO -------------------------//
    printf("\n<Server> Preparo il campo di gioco\n");

    //Inserimento delle righe e colonne nella memoria condivisa
    shared->rows = rows;
    shared->colums = colums;
    shared->Gettone1 = Gettone1;
    shared->Gettone2 = Gettone2;
    shared->vincitore = 0;

    //Inizializzazione della matrice (tutta vuota all'inizio)
    azzera(&matrix[0][0], rows, colums);

    printf("\n<Server> Campo di gioco pronto, gioco in corso\n");

    //Il server fa partire i client che stampano il campo da gioco
    semOp(semid, 1, 1);
    semOp(semid, 1, 1);
    
    
    //------------------------- GESTIONE DEL GIOCO -------------------------//
    int nt = 0, turn = 1, fine = 1;

    while(fine == 1){
        //Ricevo la mossa dal client 
        if(msgrcv(msqid, &mossa, siz, 0, 0) == -1){
            errExit("msgrcv failed\n");
        }
        nt ++; //Incremento il numero di mosse totali
        if(turn == 1){
            turn ++;
            matrix[mossa.posRiga][mossa.posColonna] = Gettone1; //Inserimento gettone nella matrice
        } else {
            turn --;
            matrix[mossa.posRiga][mossa.posColonna] = Gettone2; //Inserimento gettone nella matrice
        }

        //CONTROLLO VITTORIA CON FUNZIONE
        if(controlloVittoria(&matrix[0][0], shared->rows, shared->colums, Gettone1, Gettone2, mossa.posColonna) == 1){
            shared->vincitore = 1;
            fine = 0;
        }
            
        //CONTROLLO PAREGGIO
        if(nt == shared->rows * shared->colums && shared->vincitore == 0){
            shared->vincitore = 2;
            fine = 0;
        }
        
        //Avviso il client che puó stampare
        semOp(semid, 1, 1);
        semOp(semid, 2, 1);
        if(autoGame == 1)
            semOp(semid, 3, 1);
    } 
    

    //Aspetto la terminazione dei client per eliminare semafori e shared memeory
    printf("<Server> Aspetto che i Client terminino\n"); 
    semOp(semid, 0, -1); 
    printf("<Server> Client 1 terminato, aspetto per Client 2 \n"); 
    semOp(semid, 0, -1);
    printf("<Server> Client 2 terminato, FINE GIOCO\n");

    //Aspetto che termini il figlio 
    while(wait(NULL) != -1);

    //------------------------- ELIMINAZIONE SEMAFORI E SHARED MEMORY -------------------------//
    if(semctl(semid, 0, IPC_RMID, NULL) == -1)
        errExit("rimozione set semafori FALLITA!");

    free_shared_memory(shared);
    remove_shared_memory(shmid);
    //free_shared_memory(sharedMemory);
    //remove_shared_memory(shmId);

    return 0;
}

//Inizializza il campo da gioco
void azzera(char *m, int r, int c){
    int i,j;
    for(i=0; i<r; i++)
        for(j=0; j<c; j++)
          *(m + i * c + j) = ' ';
}

//Controllo vittoria
int controlloVittoria(char *m,int rows, int columns, char Gettone1, char Gettone2, int posColonna){
    //Variabili per verificare la vittoria
    int verticale1 = 0, verticale2 = 0;
    int orizzontale1 = 0, orizzontale2 = 0;

    //Controllo verticale
    for(int i = 0; i < rows; i++){
        if( *(m + i * columns + posColonna) == Gettone1 && *(m + (i + 1) * columns + posColonna) == Gettone1){
            verticale1++;
        }
        if( *(m + i * columns + posColonna) == Gettone2 && *(m + (i + 1) * columns + posColonna)== Gettone2){
            verticale2++;
        }
    }
    if(verticale1 == 3 || verticale2 == 3)
        return 1;

    //Controllo orizzontale
    for(int i = 0; i < rows; i++){
        for(int j = 0; j < columns; j++){
            if( *(m + i * columns + j) == Gettone1 && *(m + i * columns + j + 1) == Gettone1){
                orizzontale1++;
            }
            if( *(m + i * columns + j) == Gettone2 && *(m + i * columns + j + 1) == Gettone2){
                orizzontale2++;
            }
        }
        if(orizzontale1 == 3 || orizzontale2 == 3)
            return 1;
        orizzontale1 = 0;
        orizzontale2 = 0;
    }

    return 0;
}

//Gestione del segnale Ctrl^C
//DA RIVEDERE PERCHÉ NON FUNZIONA IL CONTROLLO DEI SEGNALI
void sigHandler(int sig){
    contaSegnale++;
    if(contaSegnale == 1)
        printf("\nSicuro di voler abbandonare?\n");
    if(contaSegnale == 2){
        shared->vincitore = 3;
        //semOp(semid, 1, 1);
        //semOp(semid, 3, 1);
        //semOp(semid, 2, 1);
        semOp(semid, 0, -1);
        semOp(semid, 0, -1);
        if(semctl(semid, 0, IPC_RMID, NULL) == -1)
            errExit("rimozione set semafori FALLITA!");

        free_shared_memory(shared);
        remove_shared_memory(shmid);
        exit(0);
    }     
}












