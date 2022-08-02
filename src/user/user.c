#include "../utils/utils.h"
#include "../master_book/master_book.h"
#include "user.h"
#include "../pid_list/pid_list.h"
#include "../master.h"

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include <string.h>
#include <strings.h>

/*Variabili statici per ogni processo utente*/
extern int SO_BUDGET_INIT;
extern int SO_USERS_NUM;
extern int SO_NODES_NUM;
extern int SO_REWARD;
extern int SO_RETRY;
extern int SO_MIN_TRANS_GEN_NSEC;
extern int SO_MAX_TRANS_GEN_NSEC;

static int cont_try = 0;

void usr_handler(int);
int calcola_bilancio(int, struct master_book*, int*);

void init_user(pid_t* users)
{
  int sem_id;
  int shm_id;
  int shm_nodes;

  struct nodes* nodes;
  struct master_book* book;
  struct sembuf sops;
  int bilancio_corrente = SO_BUDGET_INIT;
  int block_reached;
  struct sigaction sa;
  int queue_id;
  sigset_t mask;
  sigemptyset(&mask);
  sa.sa_flags = 0;
  sa.sa_mask = mask;
  sa.sa_handler = usr_handler;
  if (sigaction(SIGUSR1, &sa, NULL) == -1) {
    fprintf(ERR_FILE, "user u%d: cannot associate handler to SIGUSR1\n", getpid());
    exit(EXIT_FAILURE);
  }
  if (sigaction(SIGTERM, &sa, NULL) == -1) {
    fprintf(ERR_FILE, "user u%d: cannot associate handler to SIGTERM\n", getpid());
    exit(EXIT_FAILURE);
  }

  if ((sem_id = semget(getppid(), 0, 0)) == -1) {
    fprintf(ERR_FILE, "user u%d: err\n", getpid());
    exit(EXIT_FAILURE);
  }
  if ((shm_id = shmget(getppid(), 0, 0)) == -1) {
    fprintf(ERR_FILE, "user: cannot retrieve shm_id\n");
    exit(EXIT_FAILURE);
  }
  if ((shm_nodes = shmget(SHM_NODES_ID, 0, 0)) == -1) {
    fprintf(ERR_FILE, "user: cannot retrieve shm_nodes\n");
    exit(EXIT_FAILURE);
  }

  if ((nodes = attach_shm_memory(shm_nodes)) == NULL) {
    fprintf(ERR_FILE, "user u%d: the process cannot be attached to the nodes shared memory.\n", getpid());
    exit(EXIT_FAILURE);
  }

  if ((book = attach_shm_memory(shm_id)) == NULL) {
    fprintf(ERR_FILE, "user: the process cannot be attached to the master book shared memory.\n");
    exit(EXIT_FAILURE);
  }


  sops.sem_num = 0;
  sops.sem_op = -1;
  semop(sem_id, &sops, 1);

  fprintf(LOG_FILE, "u%d: waiting for green light.\n", getpid());
  sops.sem_op = 0;
  semop(sem_id, &sops, 1);


  while (cont_try < SO_RETRY)
  {
    if ((bilancio_corrente = calcola_bilancio(bilancio_corrente, book, &block_reached)) >= 2) {
      transaction* t;
      /* estrazione di un destinatario casuale*/
      int random_user = random_element(users, SO_USERS_NUM);
      /* estrazione di un nodo casuale*/
      int random_node = random_element(nodes->array, nodes->size);
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
      if ((queue_id = msgget(random_node, S_IWUSR)) == -1) {
        fprintf(ERR_FILE, "init_user u%d: message queue of node %d not found\n", getpid(), random_node);
        exit(EXIT_FAILURE);
      }
      /* creazione di una transazione e invio di tale al nodo generato*/
      t = malloc(sizeof(transaction));
      new_transaction(t, getpid(), random_user, cifra_utente, reward);


      transaction.hops = 0;
      transaction.transaction = *t;
      if (msgsnd(queue_id, &transaction, sizeof(transaction) - sizeof(unsigned int), IPC_NOWAIT) == -1) {
        if (errno != EAGAIN) {
          fprintf(ERR_FILE, "init_user u%d: recieved an unexpected error while sending transaction: %s.\n", getpid(), strerror(errno));
          if (errno == EACCES) {
            fprintf(ERR_FILE, "init_user u%d: permission missing\n", getpid());
          }
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

void usr_handler(int signal) {
  switch (signal) {
  case SIGUSR1:
  {
    time_t rawtime;
    struct tm* timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    fprintf(LOG_FILE, "%s user %d: transaction was accepted, resetting cont_try.\n", asctime(timeinfo), getpid());
    cont_try = 0;
    break;
  }
  case SIGTERM:
    fprintf(LOG_FILE, "u%d: killed by parent. Ending successfully\n", getpid());
    exit(EXIT_SUCCESS);
    break;
  default:
    fprintf(ERR_FILE, "user %d: an unexpected signal was recieved.\n", getpid());
    break;
  }
}
