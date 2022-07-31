#include "../master_book/master_book.h"
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
    /* estrazione di un destinatario casuale*/
    int random_user = random_element(users, SO_USERS_NUM);
    /* estrazione di un nodo casuale*/
    int random_node = random_element(nodes, SO_NODES_NUM);
    int cifra_utente;
    int num;
    int reward;
    struct msg transaction;

    if (random_user == -1) {
      fprintf(LOG_FILE, "init_user u%d:  all other users have terminated. Ending successfully.\n", getpid());
      exit(EXIT_SUCCESS);
    }

    if (random_node == -1) {
      fprintf(ERR_FILE, "init_user u%d: all nodes have terminated, cannot send transaction.\n", getpid());
      exit(EXIT_FAILURE);
    }

    /* generazione di un importo da inviare*/
    num = (rand() % (bilancio_corrente - 2 + 1)) + 2;
    reward = num / 100 * SO_REWARD;
    if (reward == 0)
      reward = 1;
    cifra_utente = num - reward;

    /* system V message queue */
    /* ricerca della coda di messaggi del nodo random */
    if ((ipc_id = msgget(random_node, 0)) == -1) {
      fprintf(ERR_FILE, "init_user u%d: message queue of node %d not found\n", getpid(), random_node);
      exit(EXIT_FAILURE);
    }
    /* creazione di una transazione e invio di tale al nodo generato*/
    t = malloc(sizeof(transaction));
    new_transaction(t, getpid(), random_user, cifra_utente, reward);


    transaction.hops = cont_try;
    transaction.transaction = *t;
    msgsnd(ipc_id, &transaction, sizeof(transaction) - sizeof(unsigned int), IPC_NOWAIT);
    kill(random_node, SIGUSR1);

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
