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
void stampa(char *m, int, int);
void stampaRigaPiena(int c);
int isValidInput(const char*, int); 
int inserisci(char *m, int, int);
void quit(int sig);
void sigHandler(int sig);


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

//Gettoni dei Giocatori
char Gettone1;
char Gettone2;

int contaSegnale = 0;

int main(int argc, char *argv[]) {

    int automaticGame = 0; //Variabile per far giocare il pc come avversario
    int autoGameSignal = 0;
    char asterisco[] = "-"; //Capire perchè l' * non va

    //Controllo input inseriti da linea di comando
    if(argc < 2 || argc > 3) {
        printf("\nErrore input: i dati inseriti non sono validi inserire ./client nome_utente\n");
        exit(1);
    }
    //Controllo se il client vuole giocare con l' avversario automatico
    if(argc == 3 && strcmp(asterisco, argv[2])==0){
        automaticGame = 1;
    }
    //Imposto la variabile autoGameSignal a 1 in modo che il client giochi in modo automatico
    int automatico = atoi(argv[1]);
    if(automatico == 1){
        autoGameSignal = 1;
    }
    
    //key
    key_t shmKey = 12; //Memoria condivisa
    key_t semKey = 23; //Semafori
    key_t msgKey = 10; //Coda dei messaggi
    key_t shmKey2 = 34; //Memoria condivisa per la matrice

    //set di segnali (non é inizializzato)
    sigset_t mySet;
    
    //Iniziallizzazio di mySet con tutti i segnali
    sigfillset(&mySet);
    //Rimozione del segnale SIGINT da mySet
    sigdelset(&mySet, SIGINT);
    // blocking all signals but SIGINT
    sigprocmask(SIG_SETMASK, &mySet, NULL);

    //Buffer nel quale salvo la posizione che chiedo al client per inserire il gettone
    char buffer[3];

    //------------------------- CREAZIONE SET DI SEMAFORI -------------------------//
    int semid = create_sem_set(semKey);

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
    

    //------------------------- CREAZIONE MEMEORIA CONDIVISA -------------------------//

    //memoria condivisa dove salvo la dimensione delle righe e delle colonne e una variabile per determinare il vincitore
    int shmid = alloc_shared_memory(shmKey, sizeof(struct Shared));
    struct Shared *shared = (struct Shared *)get_shared_memory(shmid, 0);

    //Memoria condivisa che viene utilizzata come una matrice
    int shmId = shmget(shmKey2, shared->rows * shared->colums * sizeof(char),  IPC_CREAT | S_IRUSR | S_IWUSR);
    int *sharedMemory = (int*)shmat(shmId, NULL, 0);
    char(*matrix)[shared->colums] = (char(*)[shared->colums])sharedMemory;

    //Avviso il server che gioca in automatico
    //Se la variabile automaticGame è a 1 allora il server deve eseguire la fork e la exec per far giocare un client in modo automatico
    if(automaticGame == 1){
        shared->vincitore = 1;
    }

    //Avviso il server che il client é arrivato
    semOp(semid, 0, 1);
    //Attendo il via del server
    semOp(semid, 1, -1);
    
    //Acquisizione Gettoni giocatori dalla memoria condivisa
    Gettone1 = shared->Gettone1;
    Gettone2 = shared->Gettone2;
    shared->vincitore = 0;


    //------------------------- GESTIONE DEI SEGNALI ------------------------//
    if(signal(SIGALRM, quit) == SIG_ERR)
        errExit("change signal handler failed");

    if(signal(SIGINT, sigHandler) == SIG_ERR)
        errExit("change signal handler failed");


    //------------------------- GESTIONE DEL GIOCO -------------------------//
    int fine = 1; 
    while(fine == 1){
        semOp(semid, 2, -1);
        //Se nessuno ha ancora vinto o pareggiato continuo
        if(shared->vincitore == 1){
            stampa(&matrix[0][0], shared->rows, shared->colums);
            printf("\n Hai perso!!\n"); 
            break;
        }
        if(shared->vincitore == 2){
            stampa(&matrix[0][0], shared->rows, shared->colums);
            printf("\nPareggio!!\n");
            break;
        }
        if(shared->vincitore == 3){
            printf("\nIl gioco é stato terminato dall'esterno\n");
            break;
            //exit(0);
        }
        stampa(&matrix[0][0], shared->rows, shared->colums);
        
        int colonna = 0;

        //Giocatore automatico genera numeri random e manda la posizione dove inserire il gettone al server
        if(autoGameSignal == 1){
            sleep(1);
            srand(time(NULL));
            int rigaValida = 0;
            int maxNum = shared->colums;
            colonna = rand() % (maxNum-0+1)+0;
            rigaValida = inserisci(&matrix[0][0], shared->rows, colonna);
            while(rigaValida == -1){
                colonna = rand() % (maxNum-0+1)+0;
                rigaValida = inserisci(&matrix[0][0], shared->rows, colonna);
            }
            //Mando la posizione del gettone al server
            mossa.posRiga = rigaValida;
            mossa.posColonna = colonna;
            if(msgsnd(msqid, &mossa, siz, 0) == -1){
                errExit("msgsnd failed\n");
            }
        }else{
            //Giocatore normale
            do{
                alarm(30); //Setto un timer di 30 secondi   
                printf("\nGiocatore %s inserisci la colonna:  ", argv[1]);
                fgets(buffer, sizeof(buffer), stdin);
                alarm(0); //Disattivo il timer
                //Verifico la validità dell'input
                colonna = isValidInput(buffer, shared->colums);
                if(colonna == -1){
                    printf("\nLa colonna non é valida\n");
                }
            }while(colonna == -1); //Continuo a chiedere la colonna fintanto che mi da un input valido

            int rigaValida = inserisci(&matrix[0][0], shared->rows, colonna); //Acquisisco la riga dove inserirla
            int okriga = 1;
            do{
                if(rigaValida != -1){
                    //Invio al server dove deve mettere il gettone attraverso la coda dei messaggi
                    mossa.posRiga = rigaValida;
                    mossa.posColonna = colonna;
                    if(msgsnd(msqid, &mossa, siz, 0) == -1){
                        errExit("msgsnd failed\n");
                    }
                    okriga = 0;
                }else {
                    //Se la colonna è piena ne chiedo una nuova
                    printf("\nLa colonna é piena, inseriscine una diversa!\n");
                    printf("\nGiocatore %s inserisci la colonna:  ", argv[1]);
                    fgets(buffer, sizeof(buffer), stdin);
                    colonna = isValidInput(buffer, shared->colums);
                    rigaValida = inserisci(&matrix[0][0], shared->rows, colonna);
                }
            }while(okriga == 1);
        }
        semOp(semid, 1, -1);
        
        stampa(&matrix[0][0], shared->rows, shared->colums);
        //Controllo se ce stata una vincita
        if(shared->vincitore == 1){
            printf("\nHai Vinto !!!\n");
            fine = 0;
        }
        //Controllo se ce stato un pareggio
        if(shared->vincitore == 2){
            printf("\nPareggio\n");
            fine = 0;
         }
         //Controllo se il gioco è stato terminato dall'esterno
         if(shared->vincitore == 3){
            printf("\nIl gioco é stato terminato dall'esterno\n");
            break;
            //exit(0);
        }
        if(autoGameSignal == 1)
            semOp(semid, 3, -1);
    }
    //Comunico al server che può terminare
    semOp(semid, 0, 1); 

    return 0;
}


//Funzione che stampa il campo di gioco
void stampa(char *m, int r, int c) {
    int i,j;
    printf("\n");
    stampaRigaPiena(c);  
     
    for(i = 0; i < r; i++){ 
        printf("   |");
        for(j = 0; j < c; j++)
            printf(" %c |", *(m + i * c + j));
        printf("\n");
        stampaRigaPiena(c);
    }
    stampaRigaPiena(c);  
    printf("   |");
    for(j = 0; j < c; j++){
        if(j >= 10){
            printf("%d |", j);
        }else{
            printf(" %d |", j);
        }
    }
    printf("\n");
    stampaRigaPiena(c); 
    printf("\n");
}

//Stampa di una riga |---|---|--...
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
int inserisci(char *m, int r, int c){
    int i;
    for(i = r-1; i >= 0; i--)
        if(*(m + i * r + c) == ' ')
          return i;
    return -1;
}

//Gestione del segnale alarm
void quit(int sig){
    printf("\nTempo scaduto!!");
    exit(1);
}

void sigHandler(int sig){
    contaSegnale++;
    printf("\nSicuro di voler abbandonare?\n");
    if(contaSegnale == 2)
        exit(0);
}

