#include "../master_book/master_book.h"
#include "user.h"
#include "../utils/utils.h"
#include "../pid_list/pid_list.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include<sys/sem.h>
#include <errno.h>
#include <string.h>
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

static int cont_try = 0;
static int nof_nodes;

void handler(int);
int calcola_bilancio(int, struct master_book*, int*);

void init_user(int* users, int* nodes)
{
  struct master_book* book;
  struct sembuf sops;
  int sem_id;
  int bilancio_corrente = SO_BUDGET_INIT;
  int block_reached;
  struct sigaction sa;
  int shm_id;
  int shm_node;
  int queue_id;
  sigset_t mask;
  nof_nodes = SO_NODES_NUM;
  sigemptyset(&mask);
  sa.sa_flags = 0;
  sa.sa_mask = mask;
  sa.sa_handler = handler;
  sigaction(SIGUSR1, &sa, NULL);

  if ((sem_id = semget(getppid(), 0, 0)) == -1) {
    fprintf(ERR_FILE, "user n%d: err\n", getpid());
    exit(EXIT_FAILURE);
  }
  if (shm_id = shmget(getppid(), 0, 0) == -1) {
    fprintf(ERR_FILE, "user: cannot retrieve shm_id from master");
    exit(EXIT_FAILURE);
  }
  if (shm_node = shmget(getppid(), 0, 0) == -1) {
    fprintf(ERR_FILE, "user: cannot retrieve shm_id from master");
    exit(EXIT_FAILURE);
  }
  if ((book = get_master_book(shm_id)) == NULL) {
    fprintf(ERR_FILE, "user: the process cannot be attached to the shared memory.\n");
    exit(EXIT_FAILURE);
  }
  /*da chiedere non ricordo cosa si dovesse implementare in user per i semafori*/
  sops.sem_num = 0;
  sops.sem_op = -1;
  semop(sem_id, &sops, 1);

  sops.sem_op = 0;
  semop(sem_id, &sops, 1);
  /*fine dubbio*/


  while (cont_try < SO_RETRY)
  {
    if ((bilancio_corrente = calcola_bilancio(bilancio_corrente, book, &block_reached)) >= 2) {
      transaction* t;
      /* estrazione di un destinatario casuale*/
      int random_user = random_element(users, SO_USERS_NUM);
      /* estrazione di un nodo casuale*/
      int random_node = random_element(&shm_node, nof_nodes);
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
      if ((queue_id = msgget(random_node, 0)) == -1) {
        fprintf(ERR_FILE, "init_user u%d: message queue of node %d not found\n", getpid(), random_node);
        exit(EXIT_FAILURE);
      }
      /* creazione di una transazione e invio di tale al nodo generato*/
      t = malloc(sizeof(transaction));
      new_transaction(t, getpid(), random_user, cifra_utente, reward);


      transaction.hops = cont_try;
      transaction.transaction = *t;
      if (msgsnd(queue_id, &transaction, sizeof(transaction) - sizeof(unsigned int), IPC_NOWAIT) == -1) {
        if (errno != EAGAIN) {
          fprintf(ERR_FILE, "init_user u%d: recieved an unexpected error while sending transaction: %s.\n", getpid(), strerror(errno));
        }
        cont_try++;
      }
      else {
        kill(random_node, SIGUSR1);
        bilancio_corrente -= (t->quantita + t->reward);
      }
      free(t);
    }
    else {
      cont_try++;
    }

    /*tempo di attesa dopo l'invio di una transazione*/
    sleep_random_from_range(SO_MIN_TRANS_GEN_NSEC, SO_MAX_TRANS_GEN_NSEC);
  }
  exit(EARLY_FAILURE);
}

int calcola_bilancio(int bilancio, struct master_book* book, int* block_reached)
{
  int i = *block_reached;
  unsigned int size = book->cursor;
  while (i < size) {
    transaction* ptr = book->blocks[i++];
    int j = 0;
    while (j++ < SO_BLOCK_SIZE - 1) {
      if (ptr->receiver == getpid()) {
        bilancio += ptr->quantita;
      }
      else if (ptr->sender == getpid()) {
        bilancio -= (ptr->quantita + ptr->reward);
      }
      ptr++;
    }
  }
  *block_reached = i;
  return bilancio;
}

void handler(int signal) {
  switch (signal) {
  case SIGUSR1:
    fprintf(LOG_FILE, "user %d: transaction was accepted, resetting cont_try.\n", getpid());
    cont_try = 0;
    break;
  case SIGTERM:
    exit(EXIT_SUCCESS);
    break;
  case SIGUSR2:
    nof_nodes++;
    break;
  default:
    fprintf(ERR_FILE, "user %d: an unexpected signal was recieved.\n", getpid());
    break;
  }
}
