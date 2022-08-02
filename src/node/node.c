#include "../utils/utils.h"
#include "../master_book/master_book.h"
#include "../pid_list/pid_list.h"

#include <sys/time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define SELF_SENDER -1
#define ACCEPT_TRANSACTION SIGUSR1


extern int SO_TP_SIZE;
extern int SO_NUM_FRIENDS;
extern int SO_MAX_TRANS_PROC_NSEC;
extern int SO_MIN_TRANS_PROC_NSEC;
extern int SO_HOPS;

/* IPC memories */
static int queue_id;
static int pipe_read;

static pid_t* friends;
static int nof_friends;

static transaction* transaction_pool;
static int nof_transaction;


void node_cleanup() {
  free(transaction_pool);
  msgctl(queue_id, IPC_RMID, NULL);
}

static void sig_handler(int sig) {
  struct msqid_ds stats;
  bzero(&stats, sizeof(struct msqid_ds));
  switch (sig) {
  case SIGTERM:
    node_cleanup();
    fprintf(LOG_FILE, "n%d: killed by parent. Ending successfully\n", getpid());
    exit(nof_transaction);
  case SIGUSR1:
  {
    int msg_num;
    if (msgctl(queue_id, IPC_STAT, &stats) < 0) {
      fprintf(ERR_FILE, "node n%d sig_handler: reading error of IPC_STAT. error %s.\n", getpid(), strerror(errno));
      node_cleanup();
      exit(EXIT_FAILURE);
    }
    msg_num = stats.msg_qnum;
    /* per risolvere il merge dei segnali, il nodo legge tutti i messaggi presenti nella coda */
    while (msg_num-- > 0) {
      fprintf(LOG_FILE, "n%d: msg_num = %d\n", getpid(), msg_num + 1);
      int chosen_friend;
      struct msg incoming;
      transaction t;
      if (msgrcv(queue_id, &incoming, sizeof(struct msg), 0, IPC_NOWAIT) == -1) {
        if (errno != EAGAIN && errno != ENOMSG) {
          fprintf(ERR_FILE, "sig_handler n%d: cannot read properly message. %s\n", getpid(), strerror(errno));
          node_cleanup();
          exit(EXIT_FAILURE);
        }
      }
      t = incoming.transaction;
      /*gestione invio transazione al master se hops raggiunti */
      if (incoming.hops == SO_HOPS) {
        int master_q;
        if ((master_q = msgget(getppid(), S_IWUSR)) == -1) {
          fprintf(ERR_FILE, "node n%d: cannot connect to master message queue with key %d.\n", getpid(), getppid());
          node_cleanup();
          exit(EXIT_FAILURE);
        }
        fprintf(LOG_FILE, "node n%d: ottenuta la coda del master con key %d e id %d\n", getpid(), getppid(), master_q);
        if (msgsnd(master_q, &t, sizeof(transaction), IPC_NOWAIT) == -1) {
          if (errno != EAGAIN) {
            fprintf(ERR_FILE, "node n%d: recieved an unexpected error while sending transaction to master: %s.\n", getpid(), strerror(errno));
          }
          fprintf(LOG_FILE, "node n%d: recieved EGAIN while trying to send transaction to master\n", getpid());
        }
        else {
          kill(getppid(), SIGUSR1);
        }
      }
      /*gestione transaction_pool piena */
      else if (nof_transaction == SO_TP_SIZE) {
        chosen_friend = random_element(friends, SO_NUM_FRIENDS);
        incoming.hops++;
        if (msgsnd(queue_id, &incoming, sizeof(transaction) - sizeof(unsigned int), IPC_NOWAIT) == -1) {
          if (errno != EAGAIN) {
            fprintf(ERR_FILE, "node n%d: recieved an unexpected error while sending transaction: %s.\n", getpid(), strerror(errno));
          }
        }

        kill(chosen_friend, SIGUSR1);
      }
      /* accetta la transazione nella sua transaction pool*/
      else {
        kill(t.sender, ACCEPT_TRANSACTION);
        transaction_pool[nof_transaction++] = t;
      }

      print_transaction(t);
    }
    break;
  }
  case SIGUSR2:
  {
    pid_t nodo_ricevuto;
    fprintf(ERR_FILE, "n%d: recieved SIGUSR2 from master. Adding friend\n", getpid());
    if (read(pipe_read, &nodo_ricevuto, sizeof(pid_t)) == -1) {
      fprintf(ERR_FILE, "node n%d: recieved an unexpected error while trying to read from pipe: %s\n", getpid(), strerror(errno));
    };
    friends = expand_list(friends, nof_friends, nof_friends + 1);
    friends[nof_friends++] = nodo_ricevuto;
    break;
  }
  /*gestione invio transazione periodico*/
  case SIGALRM:
  {
    if (nof_transaction > 0) {
      int chosen_friend;
      struct msg outcoming;
      outcoming.transaction = transaction_pool[--nof_transaction];
      chosen_friend = random_element(friends, SO_NUM_FRIENDS);
      if (msgsnd(queue_id, &outcoming, sizeof(transaction) - sizeof(unsigned int), IPC_NOWAIT) == -1) {
        if (errno != EAGAIN) {
          fprintf(ERR_FILE, "node n%d: recieved an unexpected error while sending transaction: %s.\n", getpid(), strerror(errno));
        }
      }
      kill(chosen_friend, SIGUSR1);
    }
    alarm(3);
    break;
  }
  }

}

static void generate()
{
  sigset_t mask;
  struct sigaction act;
  struct msqid_ds stats;


  /* system V message queue */
  if ((queue_id = msgget(getpid(), IPC_CREAT | IPC_EXCL | S_IWUSR | S_IRUSR)) < 0) {
    fprintf(ERR_FILE, "init_node: message queue already exists. Check ipcs and remove it with iprm -Q %d.\n", getpid());
    node_cleanup();
    exit(EXIT_FAILURE);
  }

  /* transaction pool init */
  transaction_pool = (transaction*)malloc(SO_TP_SIZE * sizeof(transaction));
  if (transaction_pool == NULL) {
    fprintf(ERR_FILE, "init_node: cannot allocate memory for transaction_pool. Check memory usage.\n");
    exit(EXIT_FAILURE);
  }
  if (SO_BLOCK_SIZE >= SO_TP_SIZE) {
    fprintf(LOG_FILE, "init_node: SO_BLOCK_SIZE >= SO_TP_SIZE. Check environmental config.\n");
    free(transaction_pool);
    exit(EXIT_FAILURE);
  }

  /* signal handling init */
  bzero(&mask, sizeof(sigset_t));
  sigemptyset(&mask);
  act.sa_handler = sig_handler;
  act.sa_flags = SA_NODEFER;
  act.sa_mask = mask;

  if (sigaction(SIGUSR1, &act, NULL) < 0) {
    fprintf(ERR_FILE, "init_node: could not associate handler to SIGUSR1.\n");
    node_cleanup();
    exit(EXIT_FAILURE);
  }
  if (sigaction(SIGTERM, &act, NULL) < 0) {
    fprintf(ERR_FILE, "init_node: could not associate handler to SIGTERM.\n");
    node_cleanup();
    exit(EXIT_FAILURE);
  }
  /*
  TODO: controllare che si possa settare dinamicamente MSGMNB
  if (msgctl(queue_id, IPC_STAT, &stats) < 0) {
    fprintf(ERR_FILE, "init_node: cannot access msgqueue stats. errno was %s.\n", strerror(errno));
    node_cleanup();
    exit(EXIT_FAILURE);
  }
  stats.msg_qbytes = sizeof(struct msg) * SO_TP_SIZE;
  if (msgctl(queue_id, IPC_SET, &stats) < 0) {
    fprintf(ERR_FILE, "init_node: cannot write msgqueue stats. errno was %s.\n", strerror(errno));
    node_cleanup();
    exit(EXIT_FAILURE);
  }
  */
}

static void get_out_of_the_pool()
{
  register int i = 0;
  while (i < SO_BLOCK_SIZE - 1)
  {
    transaction_pool[i] = transaction_pool[i + SO_BLOCK_SIZE - 1];
  }
}

static transaction* extract_block()
{
  int gain = 0;
  int i = 0;
  transaction node_transaction;
  transaction* block = calloc(SO_BLOCK_SIZE, sizeof(transaction));
  if (block == NULL) {
    fprintf(ERR_FILE, "extract_block: cannot allocate memory for a new transaction block. Check memory usage\n");
    node_cleanup();
    exit(EXIT_FAILURE);
  }
  while (i < SO_BLOCK_SIZE - 1)
  {
    /* check transazione non è presente nel libro mastro non necessario (se è nella transaction pool non è ANCORA nel libro mastro) */
    block[i] = transaction_pool[i];
    gain += block[i].reward;
    i++;
  }

  new_transaction(&node_transaction, SELF_SENDER, getpid(), gain, 0);
  block[SO_BLOCK_SIZE] = node_transaction;
  return block;
}

void simulate_processing(transaction* block, struct master_book book, int sem_id) {
  struct sembuf sops;
  sigset_t mask;


  sleep_random_from_range(SO_MIN_TRANS_PROC_NSEC, SO_MAX_TRANS_PROC_NSEC);
  sops.sem_num = 1;
  sops.sem_op = -1;
  semop(sem_id, &sops, 1);

  book.blocks[*book.size] = block;
  (*book.size)++;

  sops.sem_num = 1;
  sops.sem_op = 1;
  semop(sem_id, &sops, 1);

  get_out_of_the_pool();
}

void init_node(int* friends, int pipe_read, int shm_book_id, int shm_book_size_id)
{
  int sem_id;
  struct master_book book;

  friends = friends;
  nof_friends = SO_NUM_FRIENDS;

  generate();

  fprintf(LOG_FILE, "Node %d: completed setup\n", getpid());

  if ((sem_id = semget(getppid(), 0, 0)) == -1) {
    fprintf(ERR_FILE, "node n%d: err\n", getpid());
    exit(EXIT_FAILURE);
  }


  if ((book.blocks = attach_shm_memory(shm_book_id)) == NULL) {
    fprintf(ERR_FILE, "node: the process cannot be attached to the array shared memory.\n");
    node_cleanup();
    exit(EXIT_FAILURE);
  }

  if ((book.size = attach_shm_memory(shm_book_size_id)) == NULL) {
    fprintf(ERR_FILE, "node: the process cannot be attached to the array shared memory.\n");
    node_cleanup();
    exit(EXIT_FAILURE);
  }

  while (1)
  {
    transaction* block = NULL;
    if (nof_transaction < SO_BLOCK_SIZE) {
      pause();
      continue;
    }
    alarm(3);
    block = extract_block();
    simulate_processing(block, book, sem_id);

    free(block);
  }

}
