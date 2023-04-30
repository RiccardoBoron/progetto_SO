#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include<errno.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include "errExit.h"
#include "semaphore.h"
#include "shared_memory.h"

//------------------------- COSTANTI -------------------------//

//Dimensione massima delle righe e colonne della Matrice
#define MAXR 50
#define MAXC 50


//------------------------- PROTOTIPI DI FUNZIONI -------------------------//
void azzera(char m[MAXR][MAXC], int, int); //Inizializza la matrice



//------------------------- FUNZIONE PER LA CREAZIONE DI UN SET DI SEMAFORI -------------------------//
int create_sem_set(key_t semkey) {
    //Creazione di un set di 2 semafori
    int semid = semget(semkey, 4, IPC_CREAT | S_IRUSR | S_IWUSR);
    if(semid == -1)
        errExit("semget failed");

    //Inizializzazione del set di semafori
    union semun arg;
    unsigned short values[] = {0, 0, 1, 1};
    arg.array = values;

    if(semctl(semid, 0, SETALL, arg) == -1)
        errExit("semctl SETALL failed");

    return semid;
}

int msqid = -1; //Inizializzazione msqid



int main(int argc, char *argv[]) {

    //Creazione delle chiavi dei samafori e della memoria condivisa
    key_t shmKey = 12; //Memoria condivisa
    key_t semKey = 23; //Semafori
    key_t msgKey = 10; //Coda dei messaggi


    //Controllo input inseriti da linea di comando
    if(argc != 5) {
        printf("Errore input\n");
        exit(1);
    }

    //Controllo sulle righe e colonne della matrice (area di gioco)
    int rows = atoi(argv[1]);
    int colums = atoi(argv[2]);
    if(rows < 5 || colums < 5) {
        printf("Errore, the matrix must be min 5x5");
        exit(1);
    }

    //Acquisizione dei due gettoni di gioco 
    char Gettone1 = *argv[3];
    char Gettone2 = *argv[4];


    //Allocazione del segmento di memoria condivisa
    int shmid = alloc_shared_memory(shmKey, sizeof(struct Request));

    //Attacco del segmento di memoria condivisa
    struct Request *request = (struct Request*)get_shared_memory(shmid, 0);

    //Creazione di un set di semafori
    int semid = create_sem_set(semKey);

    //------------------------- IL SERVER ASPETTA I CLIENT -------------------------//
    printf("\n<Servre> aspetto i client\n");
    semOp(semid, 0, -1); //Attesa primo client
    printf("\n<Server> Client 1 arrivato, attendo Client 2\n");
    semOp(semid, 0, -1); //Attesa secondo client
    printf("\n<Server> Client 2 arrivato\n");

   //------------------------- INIZIALIZZAZIONE CAMPO DI GIOCO -------------------------//
    printf("\n<Server> Azzero il campo di gioco e lo inserisco nella struttura\n");
    //Inserimento delle righe e colonne nella matrice della struttura request
    request->rows = rows;
    request->colums = colums;
    request->Gettone1 = Gettone1;
    request->Gettone2 = Gettone2;
    request->vincitore = 0;
    //Inizializzazione della matrice (tutta vuota all'inizio)
    azzera(request->matrix, rows, colums);


    //------------------------- CREAZIONE CODA DEI MESSAGGI -------------------------//
    int msqid = msgget(msgKey, IPC_CREAT | S_IRUSR | S_IWUSR);
    if(msqid == -1){
        errExit("msgget failed\n");
    }

    //Struttura coda dei messaggi
    struct myMsg{
        int mtype;
        int posRiga;
        int posColonna;
    } mossa;

    ssize_t siz = sizeof(struct myMsg) - sizeof(long);
    mossa.mtype = 1;
    mossa.posRiga = 0;
    mossa.posColonna = 0;

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
            request->matrix[mossa.posRiga][mossa.posColonna] = Gettone1; //Inserimento gettone nella matrice
        } else {
            turn --;
            request->matrix[mossa.posRiga][mossa.posColonna] = Gettone2; //Inserimento gettone nella matrice
        }

        //Variabili per verificare la vittoria
        int verticale1 = 0, verticale2 = 0;
        int orizzontale1 = 0, orizzontale2 = 0;

        /* CONTROLLO SE QUALCUNO HA VINTO */
        //Controllo verticale
        for(int i = 0; i < request->rows - 1; i++){
            if(turn == 2 && request->matrix[i][mossa.posColonna] == Gettone1 && request->matrix[i+1][mossa.posColonna] == Gettone1){
                verticale1 ++;
            }
            if(turn == 1 && request->matrix[i][mossa.posColonna] == Gettone2 && request->matrix[i+1][mossa.posColonna] == Gettone2){
                verticale2 ++;
            }
        }

        //Controllo orizzontale
        for(int i = 0; i < request->rows; i++){
            for(int j = 0; j < request->colums; j++){
                if(turn == 2 && request->matrix[i][j] == Gettone1 && request->matrix[i][j+1] == Gettone1){
                    orizzontale1 ++;
                }
                if(turn == 1 && request->matrix[i][j] == Gettone2 && request->matrix[i][j+1] == Gettone2){
                    orizzontale1 ++;
                }
            }
        }

        //Comunico la vittoria
        if(verticale1 == 3 || verticale2 == 3 || orizzontale1 == 3 || orizzontale2 == 3){
            request->vincitore = 1;
            //break;
            fine = 0;
        }
        //Comunico il pareggio
        if(nt == request->rows * request->colums && request->vincitore == 0){
            request->vincitore = 2;
            //break;
            fine = 0;
        }
        
        //Avviso il client che puó stampare
        semOp(semid, 1, 1);
        semOp(semid, 2, 1);
    } 
    

    //Aspetto la terminazione dei client per eliminare semafori e shared memeory
    printf("<Server> aspetto che i client terminino\n"); 
    semOp(semid, 0, -1); 
    printf("Client 1 terminato, aspetto per Client 2 \n"); 
    semOp(semid, 0, -1);

    //------------------------- ELIMINAZIONE SEMAFORI E SHARED MEMORY -------------------------//
    if(semctl(semid, 0, IPC_RMID, NULL) == -1)
        errExit("rimozione set semafori FALLITA!");

    free_shared_memory(request);
    remove_shared_memory(shmid);

    return 0;
}

//Inizializza il campo da gioco con tutti 0 ovvero vuoto
void azzera(char m[MAXR][MAXC], int r, int c){
    int i,j;
    for(i=0; i<r; i++)
        for(j=0; j<c; j++)
          m[i][j]=' ';
}












