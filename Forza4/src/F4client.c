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
void stampa(char m[MAXR][MAXC], int, int);
void stampaRigaPiena(int c);
int isValidInput(const char*, int); 
int inserisci(char m[MAXR][MAXC], int, int);


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
//Gettoni dei Giocatori
char Gettone1;
char Gettone2;

int main(int argc, char *argv[]) {

    //Controllo input inseriti da linea di comando
    if(argc != 2) {
        printf("Errore input\n");
        exit(1);
    }

    //Creazione delle chiavi dei samafori e della memoria condivisa e dalla message queue(uguali al server)
    key_t shmKey = 12; //Memoria condivisa
    key_t semKey = 23; //Semafori
    key_t msgKey = 10; //Coda dei messaggi

    //Buffer nel quale salvo la posizione che chiedo al client per inserire il gettone
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

    //Struttura condivisa con coda dei messaggi, serve al server per inserire il gettone nella posizione giusta
    struct myMsg{
        int mtype;
        int posRiga;
        int posColonna;
    } mossa;

    ssize_t siz = sizeof(struct myMsg) - sizeof(long);
    mossa.mtype = 1;
    mossa.posRiga = 0;
    mossa.posColonna = 0;

    //Acquisizione Gettoni giocatori dalla memoria condivisa
    Gettone1 = request->Gettone1;
    Gettone2 = request->Gettone2;


    //------------------------- GESTIONE DEL GIOCO -------------------------//
    int fine = 1; 
    while(fine == 1){
        semOp(semid, 2, -1);
        //Se nessuno ha ancora vinto o pareggiato continuo
        if(request->vincitore == 1){
            printf("\n Hai perso!!\n"); 
            break;
        }
        if(request->vincitore == 2){
            printf("\nPareggio!!\n");
            break;
        }
        stampa(request->matrix, request->rows, request->colums);
        
        //Inserimento della colonna nella sruttura e invio al server
        int colonna = 0;
        do{
            //Chiedo al giocatore di inserire la colonna dove vuole far cadere il gettone   
            printf("\nGiocatore %s inserisci la colonna:  ", argv[1]);
            fgets(buffer, sizeof(buffer), stdin);
            colonna = isValidInput(buffer, request->colums); //Verifico che sia un input valido
            if(colonna == -1){
                printf("\nInvalid column input\n");
            }
        }while(colonna == -1); //Continuo a chiedere la colonna fintanto che mi da un input valido

        int rigaValida = inserisci(request->matrix, request->rows, colonna); //Acquisisco la riga dove inserirla
        int ok = 1;
        do{
            if(rigaValida != -1){
                //Invio al server dove deve mettere il gettone
                mossa.posRiga = rigaValida;
                mossa.posColonna = colonna;
                if(msgsnd(msqid, &mossa, siz, 0) == -1){
                    errExit("msgsnd failed\n");
                }
                ok = 0;
            }else {
                //Se la colonna è piana ne chiedo una nuova
                printf("\nLa colonna é piena!! Inseriscine una diversa!\n");
                printf("\nGiocatore %s inserisci la colonna:  ", argv[1]);
                fgets(buffer, sizeof(buffer), stdin);
                colonna = isValidInput(buffer, request->colums);
                rigaValida = inserisci(request->matrix, request->rows, colonna);
            }
        }while(ok == 1);
        
        semOp(semid, 1, -1);
        stampa(request->matrix, request->rows, request->colums);
        //Controllo se ce stata una vincita
        if(request->vincitore == 1){
            printf("\nHai Vinto !!!\n");
            fine = 0;
        }
        //Controllo se ce stato un pareggio
        if(request->vincitore == 2){
            printf("\nPareggio\n");
            fine = 0;
        }
    }
    semOp(semid, 0, 1); //Il server può terminare

    //------------------------- ELIMINAZIONE SEMAFORI E SHARED MEMORY -------------------------//
    if(semctl(semid, 0, IPC_RMID, NULL) == -1)
        errExit("rimozione set semafori FALLITA!");

    free_shared_memory(request);
    remove_shared_memory(shmid);

    return 0;
}


//Funzione che stampa il campo di gioco
void stampa(char m[MAXR][MAXC], int r, int c) {
    int i,j;
    printf("\n");
    stampaRigaPiena(c);  //Stampo prima riga |---|---|--...
     
    for(i=0; i<r; i++){ //Stampo le righe centrali
        printf("   |");
        for(j=0; j<c; j++)
            printf(" %c |", m[i][j]);
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
