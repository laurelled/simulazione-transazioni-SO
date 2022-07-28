#include "../utils/utils.h"
#include "../transaction.h"
#include "../list/list.h"

#include <signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#define REFUSE_TRANSACTION SIGUSR1

struct msg {
  unsigned int hops;
  transaction transaction;
};
static struct sigaction ACT_SIGUSR1_DFL;
static struct sigaction ACT_SIGTERM_DFL;

static int queue_id;
static int SO_TP_SIZE;
static int SO_MAX_TRANS_PROC_NSEC;
static int SO_MIN_TRANS_PROC_NSEC;

static list friends;
static transaction* transaction_pool;
static int nof_transaction;

static int ric = 0;

void cleanup() {
  free(transaction_pool);
  msgctl(queue_id, IPC_RMID, NULL);
}

static void sig_handler(int sig) {
  struct msqid_ds stats;
  switch (sig) {
  case SIGALRM:
    ric++;
    fprintf(LOG_FILE, "ricevuto SIGALRM\n");
    break;
  case SIGTERM:
    cleanup();
    exit(EXIT_SUCCESS);
  case SIGUSR1:
  {
    fprintf(LOG_FILE, "ricevuto SIGUSR1\n");
    msgqnum_t msg_num;
    if (msgctl(queue_id, IPC_STAT, &stats) < 0) {
      fprintf(ERR_FILE, "sig_handler: reading error of IPC_STAT. Check user permission.\n");
      cleanup();
      exit(EXIT_FAILURE);
    }
    msg_num = stats.msg_qnum;
    if (msg_num > 0) {
      register unsigned int i = 0;
      while (i++ < msg_num) {
        struct msg incoming;
        transaction t;

        msgrcv(queue_id, &incoming, sizeof(struct msg) - sizeof(long), 0, IPC_NOWAIT);
        t = incoming.transaction;
        if (nof_transaction == SO_TP_SIZE) {
          kill(t.sender, REFUSE_TRANSACTION);
        }
        else {
          transaction_pool[nof_transaction++] = t;
        }
        fprintf(LOG_FILE, "Recieved transaction from pid: %d\n", t.sender);
        print_transaction(t);
      }
    }
  }
  break;
  }

}

static void init_node()
{
  sigset_t mask;
  struct sigaction act;
  struct msqid_ds stats;

  /* constant retrieval from environ */
  SO_TP_SIZE = retrieve_constant("SO_TP_SIZE");
  SO_MAX_TRANS_PROC_NSEC = retrieve_constant("SO_MAX_TRANS_PROC_NSEC");
  SO_MIN_TRANS_PROC_NSEC = retrieve_constant("SO_MIN_TRANS_PROC_NSEC");

  /* transaction pool init */
  transaction_pool = (transaction*)calloc(SO_TP_SIZE, sizeof(transaction));
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

  if (sigaction(SIGUSR1, &act, &ACT_SIGUSR1_DFL) < 0) {
    fprintf(ERR_FILE, "init_node: could not associate handler to SIGUSR1.\n");
    cleanup();
    exit(EXIT_FAILURE);
  }
  if (sigaction(SIGTERM, &act, &ACT_SIGTERM_DFL) < 0) {
    fprintf(ERR_FILE, "init_node: could not associate handler to SIGTERM.\n");
    cleanup();
    exit(EXIT_FAILURE);
  }
  if (sigaction(SIGALRM, &act, NULL) < 0) {
    fprintf(ERR_FILE, "init_node: could not associate handler to SIGALRM.\n");
    cleanup();
    exit(EXIT_FAILURE);
  }

  /* system V message queue */
  if ((queue_id = msgget(getpid(), IPC_CREAT | IPC_EXCL | S_IWUSR | S_IRUSR)) < -1) {
    fprintf(ERR_FILE, "init_node: message queue already exists. Check ipcs and remove it with iprm -Q %d.\n", getpid());
    cleanup();
    exit(EXIT_FAILURE);
  }
  if (msgctl(queue_id, IPC_STAT, &stats) < 0) {
    fprintf(ERR_FILE, "init_node: cannot read msgqueue stats. Check user permission.\n");
    cleanup();
    exit(EXIT_FAILURE);
  }
  stats.msg_qbytes = sizeof(struct msg) * SO_TP_SIZE;
  if (msgctl(queue_id, IPC_SET, &stats) < 0) {
    fprintf(ERR_FILE, "init_node: cannot write msgqueue stats. Check user permission.\n");
    cleanup();
    exit(EXIT_FAILURE);
  }
}

static void shift_pool()
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
  transaction node_transaction;
  transaction* block = calloc(SO_BLOCK_SIZE, sizeof(transaction));
  if (block == NULL) {
    fprintf(ERR_FILE, "extract_block: cannot allocate memory for a new transaction block. Check memory usage\n");
    cleanup();
    exit(EXIT_FAILURE);
  }
  register int i = 0;
  while (i < SO_BLOCK_SIZE - 1)
  {
    /* TODO Check transazione non è presente nel libro mastro */
    block[i] = transaction_pool[i];
    gain += block[i].reward;
    i++;
  }

  shift_pool();

  new_transaction(&node_transaction, getpid(), SELF_RECIEVER, gain, 0);
  block[SO_BLOCK_SIZE] = node_transaction;
  return block;
}

void simulate_processing() {
  sigset_t mask;
  bzero(&mask, sizeof(sigset_t));

  sigemptyset(&mask);
  sigaddset(&mask, SIGUSR1);
  sigprocmask(SIG_BLOCK, &mask, NULL);
  sleep_random_from_range(SO_MIN_TRANS_PROC_NSEC, SO_MAX_TRANS_PROC_NSEC);
  sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

void generate_node()
{
  fprintf(LOG_FILE, "Node PID: %d\n", getpid());
  init_node();
  while (1)
  {
    transaction* block = NULL;
    if (nof_transaction < SO_BLOCK_SIZE) {
      fprintf(LOG_FILE, "ricevuti %d segnali\n", ric);
      pause();
      continue;
    }
    block = extract_block();
    /* TODO: scrittura nel libro mastro */
    simulate_processing();
    free(block);
  }
}

void handler(int sig) {
  fprintf(LOG_FILE, "SIGINT recieved\n");
  cleanup();
  exit(EXIT_SUCCESS);
}

int main() {
  int i = 0;
  signal(SIGINT, handler);
  while (i++ < 2) {
    if (fork() == 0) {
      int q;
      struct msg msg;
      transaction t;
      fprintf(LOG_FILE, "pid: %d\n", getpid());
      sleep(5);
      fprintf(LOG_FILE, "5 seconds have passed\n");
      q = msgget(getppid(), 0);
      new_transaction(&t, getpid(), 0, 100, 20);
      msg.hops = 0;
      msg.transaction = t;
      if (msgsnd(q, &msg, sizeof(struct msg) - sizeof(long), IPC_NOWAIT) < 0)
        fprintf(LOG_FILE, "PID: %d - la coda ha rifiutato il msg\n", getpid());
      fprintf(LOG_FILE, "PID: %d - sending SIGALRM to parent\n", getpid());
      kill(getppid(), SIGALRM);
      exit(EXIT_SUCCESS);
    }
  }
  generate_node();
  return 0;
}