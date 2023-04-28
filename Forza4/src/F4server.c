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

//Costanti per il gioco
#define g1 'X' //Carattere del giocatore 1
#define g2 'O' //Carattere del giocatore 2
#define vuoto ' '  //Carattere per uno spazio vuoto
#define vuotoI 0
#define g1I 1
#define g2I 2




//------------------------- PROTOTIPI DI FUNZIONI -------------------------//
void azzera(int m[MAXR][MAXC], int, int); //Inizializza la matrice
//int controllaF(int m[MAXR][MAXC], const int g, const int r, const int c, const int e); //Controlla se qualcuno ha vinto



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

    //Controll sulle righe e colonne della matrice (area di gioco)
    int rows = atoi(argv[1]);
    int colums = atoi(argv[2]);
    if(rows < 5 || colums < 5) {
        printf("Errore, the matrix must be min 5x5");
        exit(1);
    }


    //Allocazione del segmento di memoria condivisa
    int shmid = alloc_shared_memory(shmKey, sizeof(struct Request));

    //Attacco del segmento di memoria condivisa
    struct Request *request = (struct Request*)get_shared_memory(shmid, 0);

    //Creazione di un set di semafori
    int semid = create_sem_set(semKey);

    //------------------------- IL SERVER ASPETTA I CLIENT -------------------------//
    printf("<Servre> aspetto i client\n");
    semOp(semid, 0, -1); //Attesa primo client
    printf("<Server> Client 1 arrivato, attendo Client 2\n");
    semOp(semid, 0, -1); //Attesa secondo client
    printf("<Server> Client 2 arrivato\n");

   //------------------------- INIZIALIZZAZIONE CAMPO DI GIOCO -------------------------//
    printf("<Server> Azzero il campo di gioco e lo inserisco nella struttura\n");
    //Inserimento delle righe e colonne nella matrice della struttura request
    request->rows = rows;
    request->colums = colums;
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
    int nt = 0, turn = 3;
    while(1){
        //Ricevo la mossa dal client 
        if(msgrcv(msqid, &mossa, siz, 0, 0) == -1){
            errExit("msgrcv failed\n");
        }
        nt ++; //Incremento il numero di mosse totali
        if(turn == 3){
            turn ++; 
            request->matrix[mossa.posRiga][mossa.posColonna] = g1I; //Inserimento gettone nella matrice
        } else {
            turn --;
            request->matrix[mossa.posRiga][mossa.posColonna] = g2I; //Inserimento gettone nella matrice
        }

        //Variabili per verificare la vittoria
        int verticale1 = 0, verticale2 = 0;
        int orizzontale1 = 0, orizzontale2 = 0;

        /* CONTROLLO SE QUALCUNO HA VINTO */
        //Controllo verticale
        for(int i = 0; i < request->rows - 1; i++){
            if(turn == 4 && request->matrix[i][mossa.posColonna] == g1I && request->matrix[i+1][mossa.posColonna] == g1I){
                verticale1 ++;
            }
            if(turn == 3 && request->matrix[i][mossa.posColonna] == g2I && request->matrix[i+1][mossa.posColonna] == g2I){
                verticale2 ++;
            }
        }

        //Controllo orizzontale
        for(int i = 0; i < request->rows; i++){
            for(int j = 0; j < request->colums; j++){
                if(turn == 4 && request->matrix[i][j] == g1I && request->matrix[i][j+1] == g1I){
                    orizzontale1 ++;
                }
                if(turn == 3 && request->matrix[i][j] == g2I && request->matrix[i][j+1] == g2I){
                    orizzontale1 ++;
                }
            }
        }

        //Comunico la vittoria
        if(verticale1 == 2 || verticale2 == 2 || orizzontale1 == 2 || orizzontale2 == 2){
            request->vincitore = 1;
            break;
        }
        //Comunico il pareggio
        if(nt == request->rows * request->colums && request->vincitore == 0){
            request->vincitore = 2;
            break;
        }
        
        //Avviso il client che puó stampare
        semOp(semid, 1, 1);
        semOp(semid, 2, 1);
    } 
    


    //Aspetto la terminazione dei client per eliminare semafori e shared memeory
    printf("<Server> is waiting for a <Client> ending...\n"); 
    semOp(semid, 0, -1); 
    printf("1st <Client> ended, now <Server> is waiting for the 2nd. \n"); 
    semOp(semid, 0, -1);

    //------------------------- ELIMINAZIONE SEMAFORI E SHARED MEMORY -------------------------//
    if(semctl(semid, 0, IPC_RMID, NULL) == -1)
        errExit("rimozione set semafori FALLITA!");

    free_shared_memory(request);
    remove_shared_memory(shmid);


        
    return 0;
}

//Inizializza il campo da gioco con tutti 0 ovvero vuoto
void azzera(int m[MAXR][MAXC], int r, int c){
    int i,j;
    for(i=0; i<r; i++)
        for(j=0; j<c; j++)
          m[i][j]=0;
}









