#include "../transaction.h"
#include "../utils/utils.h"
#include "../pid_list/pid_list.h"

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


/*Variabili statici per ogni processo utente*/
extern int SO_BUDGET_INIT;
extern int SO_USERS_NUM;
extern int SO_NODES_NUM;
extern int SO_REWARD;
extern int SO_RETRY;
extern int SO_MIN_TRANS_GEN_NSEC;
extern int SO_MAX_TRANS_GEN_NSEC;

static int ipc_id;
static int cont_try = 0;


struct msg {
  unsigned int hops;
  transaction transaction;
};
void handler(int);
int calcola_bilancio();
void init_user(int* users, int* nodes)
{
  int bilancio_corrente = 0;
  struct sigaction sa;
  sigset_t mask;
  sigemptyset(&mask);
  sa.sa_flags = 0;
  sa.sa_mask = mask;
  sa.sa_handler = handler;
  sigaction(SIGUSR1, &sa, NULL);
  while ((bilancio_corrente = calcola_bilancio()) >= 2)
  {
    transaction* t;
    int random_user = (rand() % (SO_USERS_NUM + 1));
    int random_node;
    int cifra_utente;
    int num;
    int reward_t;
    struct msg transaction;
    cont_try = 0;
    /* estrazione di un destinatario casuale*/

    while (getpid() == users[random_user] || (kill(users[random_user], 0) == -1 && errno == ESRCH))

    { /* evita di estrarre lo stesso processo in cui ci troviamo o di trovare un utente terminato*/
      random_user = (rand() % (SO_USERS_NUM + 1));

    }
    /* estrazione di un nodo casuale*/
    random_node = (rand() % (SO_NODES_NUM + 1));
    /* generazione di un importo da inviare*/
    num = (rand() % (bilancio_corrente - 2 + 1)) + 2;
    reward_t = num / 100 * SO_REWARD;
    if (reward_t == 0)
      reward_t = 1;
    cifra_utente = num - reward_t;

    /* system V message queue */
    /* ricerca della coda di messaggi del nodo random */
    if ((ipc_id = msgget(nodes[random_node], 0)) == -1) {
      fprintf(ERR_FILE, "user %d: message queue of node %d not found\n", getpid(), nodes[random_node]);
      exit(EXIT_FAILURE);
    }
    /* creazione di una transazione e invio di tale al nodo generato*/
    t = malloc(sizeof(transaction));
    new_transaction(t, getpid(), users[random_user], cifra_utente, reward_t);


    transaction.hops = 0;
    transaction.transaction = *t;
    msgsnd(ipc_id, &transaction, sizeof(transaction) - sizeof(unsigned int), IPC_NOWAIT);
    kill(nodes[random_node], SIGUSR1);
    /*tempo di attesa dopo l'invio di una transazione*/

    sigaddset(&mask, SIGUSR1);

    sigprocmask(SIG_UNBLOCK, &mask, NULL);

    sleep_random_from_range(SO_MIN_TRANS_GEN_NSEC, SO_MAX_TRANS_GEN_NSEC);


    sigprocmask(SIG_BLOCK, &mask, NULL);

  }
}

int calcola_bilancio()
{
  int bilancio = SO_BUDGET_INIT;
  /*TODO calcolo degli importi da libro mastro e transazioni inviati*/
  return bilancio;
}

void handler(int signal) {
  switch (signal) {
  case SIGUSR1:
    cont_try++;
    break;
  default:
    break;
  }
}





