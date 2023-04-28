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
#define CVitt  'V'   
#define vuotoI 0
#define g1I 1
#define g2I 2


//------------------------- PROTOTIPI DI FUNZIONI -------------------------//
void stampa(int m[MAXR][MAXC], int, int);
void stampaRigaPiena(int c);
char stampaCasella(int);
int isValidInput(const char*, int); 
int inserisci(int m[MAXR][MAXC], int, int);

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

    //Controllo input inseriti da linea di comando
    if(argc != 2) {
        printf("Errore input\n");
        exit(1);
    }

    //Creazione delle chiavi dei samafori e della memoria condivisa (uguali al server)
    key_t shmKey = 12;
    key_t semKey = 23;
    key_t msgKey = 10; //Coda dei messaggi

    //Buffer per salvare la colonna da inserire
    char buffer[3];

    //------------------------- CREAZIONE SET DI SEMAFORI -------------------------//
    int semid = create_sem_set(semKey);

    //------------------------- ATTESA SECONDO GIOCATORE -------------------------//

    //Avviso il server che il client é arrivato
    semOp(semid, 0, 1);

    //Attendo il via del server
    semOp(semid, 1, -1);

    //------------------------- CREAZIONE MEMEORIA CONDIVISA -------------------------//
    //Acquisisco il segmento di memoria condivisa del server
    printf("<Client> getting the server's shared memory segment...\n");
    int shmid = alloc_shared_memory(shmKey, sizeof(struct Request));

    //Attacco il segmento di memoria condivisa
    printf("<Client> attaching the server's shared memeory segment...\n");
    struct Request *request = (struct Request *)get_shared_memory(shmid, 0);

    //------------------------- CREAZIONE CODA DEI MESSAGGI -------------------------//
    int msqid = msgget(msgKey, IPC_CREAT | S_IRUSR | S_IWUSR);
    if(msqid == -1){
        errExit("msgget failed\n");
    }

    //Struttura condivisa con coda dei messaggi
    struct myMsg{
        int mtype;
        int posRiga;
        int posColonna;
    } mossa;

    ssize_t siz = sizeof(struct myMsg) - sizeof(long);
    mossa.mtype = 1;
    mossa.posRiga = 0;
    mossa.posColonna = 0;


    //------------------------- GESTIONE DEL GIOCO -------------------------//
    while(1){
        semOp(semid, 2, -1);
        stampa(request->matrix, request->rows, request->colums);
            
        printf("\nGiocatore %s inserisci la colonna:  ", argv[1]);
        fgets(buffer, sizeof(buffer), stdin);

        //Inserimento della colonna nella sruttura e invio al server
        int colonna = isValidInput(buffer, request->colums); //Verifico che sia un input valido
        int rigaValida = inserisci(request->matrix, request->rows, colonna); //Acquisisco la riga dove inserirla
        if(rigaValida != -1){
            //Invio al server dove deve mettere il gettone
            mossa.posRiga = rigaValida;
            mossa.posColonna = colonna;
            if(msgsnd(msqid, &mossa, siz, 0) == -1){
                errExit("msgsnd failed\n");
            }
            //request->inserimentoRiga = rigaValida;
            //request->inserimentoColonna = colonna;
        }else {
            printf("\nLa colonna é piena!! Inseriscine una diversa!\n");
        }
        
        semOp(semid, 1, -1);
        stampa(request->matrix, request->rows, request->colums);

        if(request->vincitore == 1){
            printf("\nHai Vinto !!!\n");
            break;
        }
        if(request->vincitore == 2){
            printf("\nPareggio\n");
            break;
        }
    }
    semOp(semid, 0, 1);





    //------------------------- ELIMINAZIONE SEMAFORI E SHARED MEMORY -------------------------//
    if(semctl(semid, 0, IPC_RMID, NULL) == -1)
        errExit("rimozione set semafori FALLITA!");

    free_shared_memory(request);
    remove_shared_memory(shmid);

    return 0;
}


//Funzione che stampa il campo di gioco
void stampa(int m[MAXR][MAXC], int r, int c) {
    int i,j;
    printf("\n");
    stampaRigaPiena(c);  //Stampo prima riga |---|---|--...
     
    for(i=0; i<r; i++){ //Stampo le righe centrali
        printf("   |");
        for(j=0; j<c; j++)
            printf(" %c |",stampaCasella(m[i][j]));
        printf("\n");
        stampaRigaPiena(c);
    }
    stampaRigaPiena(c);  //Stampo prima riga |---|---|--...
    printf("   |");
    for(j=0; j<c; j++){
        if(j >= 10){
            printf("%d |", j);
        }else{
            printf(" %d |", j);
        }
    }
    printf("\n");
    stampaRigaPiena(c);  //Stampo prima riga |---|---|--...
    printf("\n");
}

//Stampa di un ariga piena  |---|---|--...
void stampaRigaPiena(int c) {
    int j;
    printf("   |");
    for(j = 1; j <= c; j++)
        printf("---|");
    printf("\n");
}

//Stampa X o O nella matrice
char stampaCasella(int x){
    char c;
    switch(x){
        case 0:
            c=vuoto; 
            break;
        case 1:
            c=g1; 
            break;
        case 2:
            c=g2;
            break;
        case 3:
            c=CVitt; 
            break;
    }
    return c;
}

//Controlla se l'input inserito é valido
int isValidInput(const char *s, int coordinata) {
    char *endptr = NULL;
    errno = 0;
    int res = strtol(s, &endptr, 10);
    if(errno == 0 && *endptr == '\n' && res >= 0 && res < coordinata) {
        return res;
    } else{
        printf("Invalid input argument\n");
        exit(1);
    }
}


// Convalida la colonna, se é libera, nel caso restituisce la casella piu bassa libera
int inserisci(int m[MAXR][MAXC], int r, int c){
    int i;
    for(i=r-1; i>=0; i--)
        if(m[i][c]==vuotoI)
          return i;
    return -1;
}
