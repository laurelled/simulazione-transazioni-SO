#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <errno.h>
#include <strings.h>
#include <signal.h>
#include <stdlib.h>
#include "../transaction.h"
#include "../utils/utils.h"

//Variabili statici per ogni processo utente
extern int SO_BUDGET_INIT;
extern int SO_USERS_NUM;
extern int SO_NODES_NUM;
extern int SO_REWARD;
extern int SO_RETRY;
extern int SO_MIN_TRANS_GEN_NSEC;
extern int SO_MAX_TRANS_GEN_NSEC;
 
static int ipc_id;
static int cont_try = 0;

//dubbio, si deve fare così?
struct msg {
  unsigned int hops;
  transaction transaction;
};

transaction generate_transaction(int* utenti_da_cui_scegliere, int max_importo, int* nodi_da_cui_scegliere)
{
  int bilancio_corrente = 0;
  struct sigaction sa;
  sigset_t mask;
  setemptymask(&mask);
  sa.sa_flags = 0;
  sa.sa_mask = mask;
  sa.sa_handler = handler;
  sigaction(SIGUSR1,&sa,NULL);
  while ((bilancio_corrente = calcola_bilancio()) >= 2)
  {
    transaction* t;
    // estrazione di un destinatario casuale
    int random_user = (rand() % (SO_USERS_NUM + 1));
    while (getpid() == utenti_da_cui_scegliere[random_user])//TODO trovare un utente non terminato 

    { // evita di estrarre lo stesso processo in cui ci troviamo o di trovare un utente terminato
      random_user = (rand() % (SO_USERS_NUM + 1));
    }
    // estrazione di un nodo casuale
    int random_node = (rand() % (SO_NODES_NUM + 1));
    // generazione di un importo da inviare 
    int num = (rand() % (bilancio_corrente - 2 + 1)) + 2;
    int reward_t = num / 100 * SO_REWARD;
    if (reward_t == 0)
      reward_t = 1;
    int cifra_utente = num - reward_t;

    /* system V message queue */
    /* ricerca della coda di messaggi del nodo random */
    if(ipc_id = msgget(nodi_da_cui_scegliere[random_node], 0 ) == -1){ 
      fprintf(ERR_FILE, "user %d: message queue of node %d not found\n", getpid(), nodi_da_cui_scegliere[random_node]);
      exit(EXIT_FAILURE);
    }
    // creazione di una transazione e invio di tale al nodo generato
    t = malloc(sizeof(transaction));
    new_transaction(&t, getpid(), utenti_da_cui_scegliere[random_user], cifra_utente, reward_t);
    //TODO invio della transazione al nodo
    struct msg transaction = {0, *t};
    msgsnd(ipc_id, &transaction, sizeof(transaction)-sizeof(unsigned int), IPC_NOWAIT);
    kill(nodi_da_cui_scegliere[random_node],SIGUSR1);
    // tempo di attesa dopo l'invio di una transazione
    
    sigaddset(SIGUSR1,&mask);
    //unblock sigprocmask 
    sigprocmask(SIG_UNBLOCK,&mask,NULL);
    
    sleep_random_from_range(SO_MIN_TRANS_GEN_NSEC, SO_MAX_TRANS_GEN_NSEC);
    
    //block sigprocmask 
    sigprocmask(SIG_BLOCK,&mask,NULL);

  }
}

int calcola_bilancio()
{
  int bilancio = SO_BUDGET_INIT;
  //TODO calcolo degli importi da libro mastro e transazioni inviati
  return bilancio;
}

void handler(int signal){
  switch (signal){
    case SIGUSR1:
      cont_try++;
      break;
    default:
      break;
  }
}





