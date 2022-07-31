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


static int queue_id;
extern int SO_TP_SIZE;
extern int SO_NUM_FRIENDS;
extern int SO_MAX_TRANS_PROC_NSEC;
extern int SO_MIN_TRANS_PROC_NSEC;

static pid_t* friends;
static int nof_friends;
static transaction* transaction_pool;
static int nof_transaction;


void cleanup() {
  free(transaction_pool);
  msgctl(queue_id, IPC_RMID, NULL);
}

static void sig_handler(int sig) {
  struct msqid_ds stats;
  switch (sig) {
  case SIGTERM:
    cleanup();
    exit(nof_transaction);
  case SIGUSR1:
  {
    int msg_num;
    if (msgctl(queue_id, IPC_STAT, &stats) < 0) {
      fprintf(ERR_FILE, "sig_handler: reading error of IPC_STAT. Check user permission.\n");
      cleanup();
      exit(EXIT_FAILURE);
    }
    msg_num = stats.msg_qnum;
    if (msg_num > 0) {
      int i = 0;
      /* per risolvere il merge dei segnali, il nodo legge tutti i messaggi presenti nella coda */
      while (i++ < msg_num) {
        int chosen_friend;
        struct msg incoming;
        transaction t;

        msgrcv(queue_id, &incoming, sizeof(struct msg) - sizeof(unsigned int), 0, IPC_NOWAIT);
        if (nof_transaction == SO_TP_SIZE) {
          chosen_friend = random_element(friends, SO_NUM_FRIENDS);
          incoming.hops++;
          if (msgsnd(ipc_id, &incoming, sizeof(transaction) - sizeof(unsigned int), IPC_NOWAIT) == -1) {
            if (errno != EAGAIN) {
              fprintf(ERR_FILE, "init_node u%d: recieved an unexpected error while sending transaction: %s.\n", getpid(), strerror(ernno));
            }
          }
          kill(chosen_friend, SIGUSR1);
        }
        else {
          t = incoming.transaction;
          kill(t.sender, ACCEPT_TRANSACTION);
          transaction_pool[nof_transaction++] = t;
        }

        fprintf(LOG_FILE, "Recieved transaction from pid: %d\n", t.sender);
        print_transaction(t);
      }
    }
  }
  break;
  case SIGALRM:
  {
    if (nof_transaction > 0) {
      int chosen_friend;
      struct msg outcoming;
      outcoming.transaction = transaction_pool[--nof_transaction];
      chosen_friend = random_element(friends, SO_NUM_FRIENDS);
      if (msgsnd(ipc_id, &outcoming, sizeof(transaction) - sizeof(unsigned int), IPC_NOWAIT) == -1) {
        if (errno != EAGAIN) {
          fprintf(ERR_FILE, "init_node u%d: recieved an unexpected error while sending transaction: %s.\n", getpid(), strerror(ernno));
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
    cleanup();
    exit(EXIT_FAILURE);
  }
  if (sigaction(SIGTERM, &act, NULL) < 0) {
    fprintf(ERR_FILE, "init_node: could not associate handler to SIGTERM.\n");
    cleanup();
    exit(EXIT_FAILURE);
  }

  /* system V message queue */
  if ((queue_id = msgget(getpid(), IPC_CREAT | IPC_EXCL | S_IWUSR | S_IRUSR)) < 0) {
    fprintf(ERR_FILE, "init_node: message queue already exists. Check ipcs and remove it with iprm -Q %d.\n", getpid());
    cleanup();
    exit(EXIT_FAILURE);
  }
  if (msgctl(queue_id, IPC_STAT, &stats) < 0) {
    fprintf(ERR_FILE, "init_node: cannot read msgqueue stats. Check user permission.\n");
    cleanup();
    exit(EXIT_FAILURE);
  }
  stats.msg_qbytes = sizeof(struct msg) * (long unsigned int) SO_TP_SIZE;
  if (msgctl(queue_id, IPC_SET, &stats) < 0) {
    fprintf(ERR_FILE, "init_node: cannot write msgqueue stats. Check user permission.\n");
    cleanup();
    exit(EXIT_FAILURE);
  }
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
    cleanup();
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

void simulate_processing(transaction* block) {

  struct master_book* book;
  struct sembuf sops;
  int sem_id;
  int shm_id;
  sigset_t mask;


  if (sem_id = semget(getppid(), 1, S_IRUSR | S_IWUSR) == -1) {
    fprintf(ERR_FILE, "node: cannot retrieve sem_id from master");
    cleanup();
    exit(EXIT_FAILURE);
  }

  if (shm_id == shmget(getppid(), 0, 0) == -1) {
    fprintf(ERR_FILE, "node: cannot retrieve shm_id from master");
    cleanup();
    exit(EXIT_FAILURE);
  }

  if ((book = get_master_book(shm_id)) == NULL) {
    fprintf(ERR_FILE, "node: the process cannot be attached to the shared memory.\n");
    cleanup();
    exit(EXIT_FAILURE);
  }
  sleep_random_from_range(SO_MIN_TRANS_PROC_NSEC, SO_MAX_TRANS_PROC_NSEC);
  sops.sem_num = 1;
  sops.sem_op = -1;
  semop(sem_id, &sops, 1);

  book->blocks[book->cursor] = block;
  book->cursor++;

  sops.sem_num = 1;
  sops.sem_op = 1;
  semop(sem_id, &sops, 1);
  get_out_of_the_pool();
}

void init_node()
{
  fprintf(LOG_FILE, "Node PID: %d\n", getpid());
  generate();

  while (1)
  {
    transaction* block = NULL;
    if (nof_transaction < SO_BLOCK_SIZE) {
      pause();
      continue;
    }
    alarm(3);
    block = extract_block();
    simulate_processing(block);

    free(block);
  }

}
