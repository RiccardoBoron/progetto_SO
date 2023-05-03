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
#include "errExit.h"
#include "semaphore.h"
#include "shared_memory.h"

//------------------------- COSTANTI -------------------------//
//Dimensione massima delle righe e colonne della Matrice
#define MAXR 50
#define MAXC 50

//------------------------- PROTOTIPI DI FUNZIONI -------------------------//
void azzera(char m[MAXR][MAXC], int, int); //Inizializza la matrice
int isValidInput(const char*, int); //valida l'input
int inserisci(char m[MAXR][MAXC], int, int); //Restituisce la posizione di inserimento del gettone
int controlloVittoria(char m[MAXR][MAXR], int rows, int columns, char Gettone1, char Gettone2, int posColonna, int turn); //Controlla la vittori


//------------------------- FUNZIONE PER LA CREAZIONE DI UN SET DI SEMAFORI -------------------------//
int create_sem_set(key_t semkey) {
    //Creazione di un set di 2 semafori
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
        printf("\nErrore, il campo di gioco deve essere almeno 5x5\n");
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
    printf("\n<Servre> Aspetto i client\n");
    semOp(semid, 0, -1); //Attesa primo client
    printf("\n<Server> Client 1 arrivato, attendo Client 2\n");
    semOp(semid, 0, -1); //Attesa secondo client
    printf("\n<Server> Client 2 arrivato\n");

   //------------------------- INIZIALIZZAZIONE CAMPO DI GIOCO -------------------------//
    printf("\n<Server> Preparo il campo di gioco\n");
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
        int automaticGame;
    } mossa;

    ssize_t siz = sizeof(struct myMsg) - sizeof(long);
    mossa.mtype = 1;
    mossa.posRiga = 0;
    mossa.posColonna = 0;
    mossa.automaticGame = 0;

    
    //Il server fa partire i client che stampano il campo da gioco
    semOp(semid, 1, 1); 
    semOp(semid, 1, 1);


    //Aspetto che i client mi dicano se devo giocare in modalitá automaticGame
    int autoGame = 0;
    if(msgrcv(msqid, &mossa, siz, 0, 0) == -1){
        errExit("msgrcv failed\n");
    }else if(mossa.automaticGame == 1){
        autoGame = 1;
    }
    //Sblocco il client 1
    semOp(semid, 3, 1);
    if(msgrcv(msqid, &mossa, siz, 0, 0) == -1){
        errExit("msgrcv failed\n");
    }else if(mossa.automaticGame == 1){
        autoGame = 1;
    }
    //Sblocco il client 2
    semOp(semid, 3, 1);

    
    //------------------------- GESTIONE DEL GIOCO -------------------------//
    int nt = 0, turn = 1, fine = 1;

    //DUPLICO IL SERVER IN MODO CHE GIOCHI IN MODALITA AUTOGAME
    if(autoGame == 1){
        pid_t pid = fork();
        if(pid == -1){
            errExit("\nErrore creazione del figlio\n");
        }else if(pid == 0){
            
            srand(time(NULL));
            int colonna = 0;
            int rigaValida = 0;
            int maxNum = request->colums;
            while(fine == 1){
                if(msgrcv(msqid, &mossa, siz, 0, 0) == -1){
                    errExit("msgrcv failed\n");
                }
                if(mossa.automaticGame == 1){
                    //Genero colonne casuali
                    nt++;
                    colonna = rand() % (maxNum-0+1)+0;
                    rigaValida = inserisci(request->matrix, request->rows, colonna);
                    while(rigaValida == -1){
                        colonna = rand() % (maxNum-0+1)+0;
                        rigaValida = inserisci(request->matrix, request->rows, colonna);
                    }
                    if(turn == 1){
                        turn ++; 
                        request->matrix[rigaValida][colonna] = Gettone1; //Inserimento gettone nella matrice
                    } else {
                        turn --;
                        request->matrix[rigaValida][colonna] = Gettone2; //Inserimento gettone nella matrice
                    }
                }else{
                    //Giocatore umano
                    nt ++; //Incremento il numero di mosse totali
                    if(turn == 1){
                        turn ++; 
                        request->matrix[mossa.posRiga][mossa.posColonna] = Gettone1; //Inserimento gettone nella matrice
                    } else {
                        turn --;
                        request->matrix[mossa.posRiga][mossa.posColonna] = Gettone2; //Inserimento gettone nella matrice
                    }
                }

                //CONTROLLO VITTORIA CON FUNZIONE
                if(controlloVittoria(request->matrix, request->rows, request->colums, Gettone1, Gettone2, mossa.posColonna, turn) == 1){
                    request->vincitore = 1;
                    fine = 0;
                }

                //CONTROLLO IL PAREGGIO
                if(nt == request->rows * request->colums && request->vincitore == 0){
                    request->vincitore = 2;
                    fine = 0;
                }
        
                //Avviso il client che puó stampare
                semOp(semid, 1, 1);
                semOp(semid, 2, 1);
            }
            exit(0);
        }
        while(wait(NULL) != -1);
    }else{
        //MODALITA NORMALE
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

            //CONTROLLO VITTORIA CON FUNZIONE
            if(controlloVittoria(request->matrix, request->rows, request->colums, Gettone1, Gettone2, mossa.posColonna, turn) == 1){
                request->vincitore = 1;
                fine = 0;
            }
            
            //CONTROLLO PAREGGIO
            if(nt == request->rows * request->colums && request->vincitore == 0){
                request->vincitore = 2;
                fine = 0;
            }
        
            //Avviso il client che puó stampare
            semOp(semid, 1, 1);
            semOp(semid, 2, 1);
        } 
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

//Controlla se l'input inserito é valido
int isValidInput(const char *s, int coordinata) {
    char *endptr = NULL;
    errno = 0;
    int res = strtol(s, &endptr, 10);
    if(errno == 0 && *endptr == '\n' && res >= 0 && res < coordinata) {
        return res;
    } else{
        return -1;
    }
}

// Convalida la colonna, se é libera, nel caso restituisce la casella piu bassa libera
int inserisci(char m[MAXR][MAXC], int r, int c){
    int i;
    for(i=r-1; i>=0; i--)
        if(m[i][c]==' ')
          return i;
    return -1;
}

//Controllo vittoria
int controlloVittoria(char m[MAXR][MAXR],int rows, int columns, char Gettone1, char Gettone2, int posColonna, int turn){
    //Variabili per verificare la vittoria
    int verticale1 = 0, verticale2 = 0;
    int orizzontale1 = 0, orizzontale2 = 0;

    //Controllo verticale
    for(int i = 0; i < rows; i++){
        if(turn == 2 && m[i][posColonna] == Gettone1 && m[i+1][posColonna] == Gettone1){
            verticale1++;
        }
        if(turn == 1 && m[i][posColonna] == Gettone2 && m[i+1][posColonna] == Gettone2){
            verticale2++;
        }
    }

    //Controllo orizzontale
    for(int i = 0; i < rows; i++){
        for(int j = 0; j < columns; j++){
            if(turn == 2 && m[i][j] == Gettone1 && m[i][j+1] == Gettone1){
                orizzontale1 ++;
            }
            if(turn == 1 && m[i][j] == Gettone2 && m[i][j+1] == Gettone2){
                orizzontale2++;
            }
        }
    }

    //Ritorno 1 se qualcuno ha vinto, 0 se nessuno ha vinto
    if(verticale1 == 3 || verticale2 == 3 || orizzontale1 == 3 || orizzontale2 == 3)
        return 1;

    return 0;
}












