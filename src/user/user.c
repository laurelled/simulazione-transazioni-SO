#include <stdio.h>
#include <unistd.h>
#include "../transaction.h"
#include "../utils/utils.h"

static int SO_BUDGET_INIT = 0;
static int SO_USERS_NUM = 0;
static int SO_NODES_NUM = 0;
static int SO_REWARD = 0;
static int SO_MIN_TRANS_GEN_NSEC = 0;
static int SO_MAX_TRANS_GEN_NSEC = 0;

transaction generate_transaction(int* utenti_da_cui_scegliere, int max_importo, int* nodi_da_cui_scegliere)
{
  SO_BUDGET_INIT = retrieve_constant("SO_BUDGET_INIT");
  SO_USERS_NUM = retrieve_constant("SO_USERS_NUM");
  SO_NODES_NUM = retrieve_constant("SO_NODES_NUM");
  SO_REWARD = retrieve_constant("SO_REWARD");
  SO_MIN_TRANS_GEN_NSEC = retrieve_constant("SO_MIN_TRANS_GEN_NSEC");
  SO_MAX_TRANS_GEN_NSEC = retrieve_constant("SO_MAX_TRANS_GEN_NSEC");


  int bilancio_corrente = calcola_bilancio();
  while (bilancio_corrente = calcola_bilancio() >= 2)
  {
    transaction t;
    // estrazione di un destinatario casuale
    int random_user = (rand() % (SO_USERS_NUM + 1));
    while (getpid() == utenti_da_cui_scegliere[random_user])
    { // evita di estrarre lo stesso processo in cui ci troviamo
      random_user = (rand() % (SO_USERS_NUM + 1));
    }
    // estrazione di un nodo casuale
    int random_node = (rand() % (SO_NODES_NUM + 1));
    // generazione di un importo da inviare 
    int num = (rand() % (bilancio_corrente - 2 + 1)) + 2;
    int cifra_nodo = num / 100 * SO_REWARD;
    if (cifra_nodo == 0)
      cifra_nodo = 1;
    int cifra_utente = bilancio_corrente - cifra_nodo;

    // creazione di una transazione e invio di tale al nodo generato
    new_transaction(&t, getpid(), utenti_da_cui_scegliere[random_user], cifra_utente, cifra_nodo);
    //TODO invio della transazione al nodo


    // tempo di attesa dopo l'invio di una transazione
    long maxT = retrieve_constant("SO_MAX_TRANS_GEN_NSEC");
    long minT = retrieve_constant("SO_MIN_TRANS_GEN_NSEC");
    sleep(rand() % (maxT - minT + 1) + minT);
  }
}

int calcola_bilancio()
{
  int bilancio = SO_BUDGET_INIT;
  //TODO calcolo degli importi da libro mastro e transazioni inviati
  return bilancio;
}
