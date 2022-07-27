#include <stdio.h>
#include <unistd.h>
#include "../transaction.h"
#include "../retrieve_constant.h"

static int SO_BUDGET_INIT = retrieve_constant("SO_BUDGET_INIT");

transaction generate_transaction(int[] utenti_da_cui_scegliere, int max_importo, int[] nodi_da_cui_scegliere)
{
  int bilancio_corrente = calcola_bilancio();
  while (bilancio_corrente = calcola_bilancio() >= 2)
  {
    // estrazione di un destinatario casuale
    int max_users = retrieve_constant("SO_USERS_NUM");
    int random_user = (rand() % (max_users + 1));
    while (getpid() == getpid(utenti_da_cui_scegliere[random_user]))
    { // evita di estrarre lo stesso processo in cui ci troviamo
      random_user = (rand() % (max_users + 1));
    }
    // estrazione di un nodo casuale
    int max_nodes = retrieve_constant("SO_NODES_NUM");
    int random_node = (rand() % (max_nodes + 1));
    // generazione di un importo da inviare 
    int num = (rand() % (bilancio_corrente - 2 + 1)) + 2;
    int rew = retrieve_constant("SO_REWARD");
    int cifra_nodo = num / 100 * rew;
    if (cifra_nodo == 0)
      cifra_nodo = 1;
    int cifra_utente = bilancio_corrente - cifra_nodo;

    // creazione di una transazione e invio di tale al nodo generato
    transaction *t = new_transaction(getpid(), getpid(utenti_da_cui_scegliere[random_user]), cifra_utente, cifra_nodo);
    //TODO invio della transazione al nodo


    // tempo di attesa dopo l'invio di una transazione
    long maxT = retrieve_constant("SO_MAX_TRANS_GEN_NSEC");
    long minT = retrieve_constant("SO_MIN_TRANS_GEN_NSEC");
    sleep(rand() % (max - min + 1) + min);
  }
}

int calcola_bilancio()
{
  int bilancio = SO_BUDGET_INIT;
  //TODO calcolo degli importi da libro mastro e transazioni inviati
  return bilancio;
}
