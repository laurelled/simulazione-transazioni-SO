#include <stdio.h>
#include <unistd.h>
#include "../transaction.h"
#include "../utils/utils.h"

//Variabili statici per ogni processo utente
extern int SO_BUDGET_INIT;
extern int SO_USERS_NUM;
extern int SO_NODES_NUM;
extern int SO_REWARD;
extern int SO_MIN_TRANS_GEN_NSEC;
extern int SO_MAX_TRANS_GEN_NSEC;

transaction generate_transaction(int* utenti_da_cui_scegliere, int max_importo, int* nodi_da_cui_scegliere)
{
  int bilancio_corrente = 0;


  while ((bilancio_corrente = calcola_bilancio()) >= 2)
  {
    transaction t;
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

    // creazione di una transazione e invio di tale al nodo generato
    new_transaction(&t, getpid(), utenti_da_cui_scegliere[random_user], cifra_utente, reward_t);
    //TODO invio della transazione al nodo


    // tempo di attesa dopo l'invio di una transazione
    //block sigprocmask

    sleep_random_from_range(SO_MIN_TRANS_GEN_NSEC, SO_MAX_TRANS_GEN_NSEC);

    //unblock sigprocmask
    //kill(getppid(), SIGUSR1);
  }
}

int calcola_bilancio()
{
  int bilancio = SO_BUDGET_INIT;
  //TODO calcolo degli importi da libro mastro e transazioni inviati
  return bilancio;
}
