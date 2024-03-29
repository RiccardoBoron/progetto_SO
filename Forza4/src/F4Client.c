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
#include "errExit.h"
#include "semaphore.h"
#include "shared_memory.h"
#include "message_queue.h"



//------------------------- PROTOTIPI DI FUNZIONI -------------------------//
void stampa(char *m, int, int);
void stampaRiga(int c);
int isValidInput(const char*, int); 
int inserisci(char *m, int, int);
void sigAlarm(int sig);
void sigHandler(int sig);
void quitCtrl();
void quitCtrlClient(int sig);
void terminalSignal(int sig);


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


int msqid = -1; 
int semid = 0;
struct Shared *shared;
struct myMsg mossa;
ssize_t siz = sizeof(struct myMsg) - sizeof(long);

int main(int argc, char *argv[]) {

    //Variabili per il giocatore automatico
    int automaticGame = 0; 
    int autoGameSignal = 0;
    
    char asterisco[] = "*";

    //Controllo input inseriti da linea di comando
    if(argc < 2 || argc > 3) {
        printf("\nErrore input: i dati inseriti non sono validi inserire ./F4client nome_utente\n");
        exit(1);
    }

    //Controllo se il client vuole giocare con l' avversario automatico
    if(argc == 3 && strcmp(asterisco, argv[2])==0){
        automaticGame = 1;
    }
    
    //Imposto la variabile autoGameSignal a 1 in modo che il client giochi in modo automatico
    if(argc == 2 && atoi(argv[1]) == 1 && strcmp("giocatore", argv[0]) == 0){
        autoGameSignal = 1;
    }

    
    //key
    key_t shmKey = 12; //Memoria condivisa
    key_t semKey = 23; //Semafori
    key_t msgKey = 10; //Coda dei messaggi
    key_t shmKey2 = 34; //Memoria condivisa per la matrice

    //set di segnali
    sigset_t mySet;
    //Iniziallizzazio di mySet con tutti i segnali
    sigfillset(&mySet);
    //Rimozione del segnale SIGINT, SIGALARM, SIGUSR1, SIGHUP da mySet
    sigdelset(&mySet, SIGINT);
    sigdelset(&mySet, SIGALRM);
    sigdelset(&mySet, SIGUSR1);
    sigdelset(&mySet, SIGHUP);
    //blocco tutti gli altri segnali su mySet
    sigprocmask(SIG_SETMASK, &mySet, NULL);

    //Buffer nel quale salvo la posizione che chiedo al client per inserire il gettone
    char buffer[100];

    //------------------------- CREAZIONE SET DI SEMAFORI -------------------------//
    semid = create_sem_set(semKey);

    //------------------------- CREAZIONE CODA DEI MESSAGGI -------------------------//
    msqid = msgget(msgKey, IPC_CREAT | S_IRUSR | S_IWUSR);
    if(msqid == -1){
        errExit("msgget failed\n");
    }

    mossa.mtype = 1;
    mossa.posRiga = 0;
    mossa.posColonna = 0;
    
    //------------------------- CREAZIONE MEMEORIA CONDIVISA -------------------------//

    //Memoria condivisa dove salvo la dimensione delle righe e delle colonne e una variabile per determinare il vincitore
    int shmid = alloc_shared_memory(shmKey, sizeof(struct Shared));
    shared = (struct Shared *)get_shared_memory(shmid, 0);

    //Verifico se si collegano piú di due client, solo due client possono eseguire nel caso un terzo esegua termino per errore di gioco
    shared->nClient ++;
    if(shared->nClient > 2){
        kill(shared->pidServer, SIGHUP);
        errExit("\nERRORE DI GIOCO: non é possibile giocare in 3 !!!\n");
    }

    //Verifico se un client viene eseguito prima del server, in caso positivo genera un errrore di gioco
    if(shared->pidServer == 0){
        //Rimuovo le ipc create

        //Semafori
        if(semctl(semid, 0, IPC_RMID, NULL) == -1)
            errExit("smctl failed\n");
        
        //Memoria condivisa
        free_shared_memory(shared);

        remove_shared_memory(shmid);

        //Coda dei messaggi
        if(msgctl(msqid, IPC_RMID, NULL) == -1)
            errExit("msgctl IPC_RMID failed");

        errExit("ERRORE DI GIOCO: Server non attivo");
    }


    //Memoria condivisa che viene utilizzata come una matrice
    int shmId = shmget(shmKey2, shared->rows * shared->colums * sizeof(char),  IPC_CREAT | S_IRUSR | S_IWUSR);
    int *sharedMemory = (int*)shmat(shmId, NULL, 0);
    char(*matrix)[shared->colums] = (char(*)[shared->colums])sharedMemory;

    //Avviso il server che gioca in automatico
    //Se la variabile automaticGame è a 1 allora il server deve eseguire la fork e la exec per far giocare un client in modo automatico
    if(automaticGame == 1){
        shared->autoGameFlag = 1;
    }

    //Mando il pid al server
    mossa.pidClient = getpid();
    msgSnd(msqid, &mossa, siz, 0);

    //Avviso il server che il client é arrivato
    semOp(semid, 0, 1);
    //Attendo il via del server
    semOp(semid, 1, -1);

    //Comunico i gettoni ai client
    shared->gettoneInizio ++;
    if(autoGameSignal != 1){
        if(shared->gettoneInizio == 1){
            printf("\nGiocherai con il gettone %c", shared->Gettone1);
        }else{
            printf("Giocherai con il gettore %c", shared->Gettone2);
        }
    }
    
    //------------------------- GESTIONE DEI SEGNALI ------------------------//
    if(signal(SIGALRM, sigAlarm) == SIG_ERR)
        errExit("change signal handler failed");

    if(signal(SIGINT, sigHandler) == SIG_ERR)
        errExit("change signal handler failed");

    if(signal(SIGUSR1, quitCtrlClient) == SIG_ERR)
        errExit("change signal handler failed");

    if(signal(SIGHUP, terminalSignal) == SIG_ERR)
        errExit("change signal handler failed");

    //------------------------- GESTIONE DEL GIOCO -------------------------//

    int fine = 1; 
    while(fine == 1){
        if(autoGameSignal == 1){
            semOp(semid, 3, -1);
        }else{
            semOp(semid, 2, -1);
        }
        //Se nessuno ha ancora vinto o pareggiato continuo
        if(shared->vincitore == 1){
            if(autoGameSignal != 1){
                stampa(&matrix[0][0], shared->rows, shared->colums);
                printf("\nHai perso!!\n"); 
            }
            break;
        }
        if(shared->vincitore == 2){
            if(autoGameSignal != 1){
                stampa(&matrix[0][0], shared->rows, shared->colums);
                printf("\nPareggio!!\n");
            }
            break;
        }
        if(shared->vincitore == 3){
            if(autoGameSignal != 1){
                printf("\nVittoria per abbandono\n");
            }
            msgSnd(msqid, &mossa, siz, 0);
            break;
        }
        if(autoGameSignal != 1){
            stampa(&matrix[0][0], shared->rows, shared->colums);
        }

        int colonna = 0;

        //Giocatore automatico genera numeri random e manda la posizione dove inserire il gettone al server
        if(autoGameSignal == 1){
            mossa.aUtoGame = 0;
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
            msgSnd(msqid, &mossa, siz, 0);
        }else{
            //Giocatore normale
                mossa.aUtoGame = 1;
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
                    msgSnd(msqid, &mossa, siz, 0);
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
        
        if(autoGameSignal != 1){
            stampa(&matrix[0][0], shared->rows, shared->colums);
        }
        
        //Controllo se ce stata una vincita
        if(shared->vincitore == 1){
            if(shared->autoGameFlag == 0){
                printf("\nHai Vinto!!\n");
            }

            if(shared->autoGameFlag == 1 && shared->FlagVittoriaAutogame == 1){
                if(autoGameSignal != 1)
                    printf("\nHai Perso!!\n");
                break;
            }else if(shared->autoGameFlag == 1 && shared->FlagVittoriaAutogame == 2){
                if(autoGameSignal != 1)
                    printf("\nHai Vinto!!\n");
                break;
            }
            fine = 0;
        }

        //Controllo se ce stato un pareggio
        if(shared->vincitore == 2){
            if(autoGameSignal != 1){
                printf("\nPareggio!!\n");
            }
            fine = 0;
         }

        //Controllo se vincitore è a 3 allora vuol dire che è scaduto il timer per l' inserimento della mossa
        if(shared->vincitore == 3){
            if(autoGameSignal != 1){
                printf("\nVittoria per abbandono\n");
            }
            break;
        }
    }
    //Comunico al server che può terminare
    semOp(semid, 0, 1); 

    //Detach memoria condivisa
    free_shared_memory(shared);

    return 0;
}


//Funzione che stampa il campo di gioco
void stampa(char *m, int r, int c) {
    int i,j;
    printf("\n");
    stampaRiga(c);  
     
    for(i = 0; i < r; i++){ 
        printf("   |");
        for(j = 0; j < c; j++)
            printf(" %c |", *(m + i * c + j));
        printf("\n");
        stampaRiga(c);
    }
    stampaRiga(c);  
    printf("   |");
    for(j = 0; j < c; j++){
        if(j >= 10){
            printf("%d |", j);
        }else{
            printf(" %d |", j);
        }
    }
    printf("\n");
    stampaRiga(c); 
    printf("\n");
}

void stampaRiga(int c) {
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

//Convalida la colonna, se é libera, nel caso restituisce la casella piu bassa libera
int inserisci(char *m, int r, int c){
    int i;
    int colonne = shared->colums;
    if(r == colonne){
        for(i = r-1; i >= 0; i--)
            if(*(m + i * r + c) == ' '){
                return i;
            }       
        return -1;
    }else{
        for(i = r-1; i >= 0; i--)
            if(*(m + i * colonne + c) == ' '){
                return i;
            }       
        return -1;
    }
}   

//Gestione del segnale alarm
void sigAlarm(int sig){
    printf("\nTempo scaduto!!\n");
    //Nel caso terminasse un giocatore fuori dal turno, altrimenti il server resterebbe bloccato nella ricezione del messaggio
    msgSnd(msqid, &mossa, siz, 0); 
    shared->vincitore = 3;
    semOp(semid, 2, 1);
    semOp(semid, 0, 1);
    //Se termina il giocatore al quale non é concesso il turno sblocco il giocatore che sta inserendo e termina
    semOp(semid, 1, 1);
    semOp(semid, 3, 1);
    //Detach shared memory
    free_shared_memory(shared);
    exit(0);
}

//Gestione del segnale ctrl+c
void sigHandler(int sig){
    if(shared->vincitore == -1){
        printf("\nIl gioco é stato terminato dall'esterno");
        free_shared_memory(shared);
        exit(0);
    }else{
        quitCtrl();
    }
}

//Avviso il server con il segnale di SIGUSR2 che un client ha abbandonato la partita
void quitCtrl(){
    pid_t pidServer = shared->pidServer;
    shared->pidServer = getpid(); //Sostituisco pidSerever con il pid del client che ha terminato
    kill(pidServer, SIGUSR2);
    //Detach shared memeory
    free_shared_memory(shared);
    exit(0);
}

//Gestione del segnare SIGUSR1
void quitCtrlClient(int sig){
    printf("\nVittoria per abbandono\n");
    //Detach shared memory
    free_shared_memory(shared);
    exit(0);
}

//Gestione del segnale di chiusura del terminale
void terminalSignal(int sig){
    quitCtrl();
}
