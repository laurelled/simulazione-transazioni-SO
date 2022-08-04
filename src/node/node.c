#include "../utils/utils.h"
#include "../master_book/master_book.h"
#include "../master/master.h"
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
#include <time.h>
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

static int* friends;
static int nof_friends;

static transaction* transaction_pool;
static int nof_transaction;


void node_cleanup() {
  free(transaction_pool);
  kill(getppid(), SIGINT);
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
      fprintf(ERR_FILE, "node n%d sig_handler: reading error of IPC_STAT of queue with id %d. error %s.\n", getpid(), queue_id, strerror(errno));
      node_cleanup();
      exit(EXIT_FAILURE);
    }
    msg_num = stats.msg_qnum;
    /* per risolvere il merge dei segnali, il nodo legge tutti i messaggi presenti nella coda */
    while (msg_num-- > 0) {
      int chosen_friend;
      struct msg incoming;
      transaction t;

      bzero(&incoming, sizeof(struct msg));

      if (msgrcv(queue_id, &incoming, sizeof(struct msg) - sizeof(long), 0, IPC_NOWAIT) == -1) {
        if (errno != EAGAIN && errno != ENOMSG) {
          fprintf(ERR_FILE, "sig_handler n%d: cannot read properly message. %s\n", getpid(), strerror(errno));
          node_cleanup();
          exit(EXIT_FAILURE);
        }
      }
      t = incoming.mtext;
      /*gestione invio transazione al master se SO_HOPS raggiunti */
      if (incoming.mtype == SO_HOPS + 1) {
        int master_q;
        if ((master_q = msgget(getppid(), 0)) == -1) {
          fprintf(ERR_FILE, "node n%d: cannot connect to master message queue with key %d.\n", getpid(), getppid());
          node_cleanup();
          exit(EXIT_FAILURE);
        }
        if (msgsnd(master_q, &t, sizeof(struct msg) - sizeof(long), IPC_NOWAIT) == -1) {
          if (errno != EAGAIN) {
            /* fprintf(ERR_FILE, "node n%d: recieved an unexpected error while sending transaction to master: %s.\n", getpid(), strerror(errno));*/
            node_cleanup();
            exit(EXIT_FAILURE);
          }
          /*fprintf(LOG_FILE, "node n%d: recieved EGAIN while trying to send transaction to master\n", getpid());*/
        }
        else {
          kill(getppid(), SIGUSR1);
        }
      }
      /*gestione transaction_pool piena */
      else if (nof_transaction == SO_TP_SIZE) {
        int friend_queue;
        chosen_friend = random_element(friends, nof_friends);
        if (chosen_friend == -1) {
          fprintf(LOG_FILE, "%s:%d: n%d: cannot choose a random friend\n", __FILE__, __LINE__, getpid());
          node_cleanup();
          CHILD_STOP_SIMULATION;
          exit(EXIT_FAILURE);
        }
        if ((friend_queue = msgget(chosen_friend, 0)) == -1) {
          fprintf(ERR_FILE, "node n%d: cannot connect to friend message queue with key %d (%s).\n", getpid(), chosen_friend, strerror(errno));
          node_cleanup();
          CHILD_STOP_SIMULATION;
          exit(EXIT_FAILURE);
        }
        incoming.mtype += 1;
        if (msgsnd(friend_queue, &incoming, sizeof(struct msg) - sizeof(long), IPC_NOWAIT) == -1) {
          if (errno != EAGAIN) {
            fprintf(ERR_FILE, "node n%d: recieved an unexpected error while sending transaction to a friend: %s.\n", getpid(), strerror(errno));
            node_cleanup();
            CHILD_STOP_SIMULATION;
            exit(EXIT_FAILURE);
          }
        }

        kill(chosen_friend, SIGUSR1);
      }
      /* accetta la transazione nella sua transaction pool*/
      else {
        kill(t.sender, ACCEPT_TRANSACTION);
        transaction_pool[nof_transaction++] = t;
        /*
        fprintf(LOG_FILE, "[%ld] n%d: mtype: %ld/%d ", clock() / CLOCKS_PER_SEC, getpid(), incoming.mtype - 1, SO_HOPS);
        print_transaction(t);
        */
      }
    }
    break;
  }
  case SIGUSR2:
  {
    int nodo_ricevuto = 0;
    if (read(pipe_read, &nodo_ricevuto, sizeof(int)) == -1) {
      if (errno != EINTR) {
        fprintf(ERR_FILE, "node n%d: recieved an unexpected error while trying to read from pipe: %s\n", getpid(), strerror(errno));
        node_cleanup();
        CHILD_STOP_SIMULATION;
        exit(EXIT_FAILURE);
      }
    };
    friends = expand_list(friends, nof_friends, nof_friends + 1);
    if (friends == NULL) {
      fprintf(LOG_FILE, "n%d: expand_list error in SIGUSR2\n", getpid());
      node_cleanup();
      CHILD_STOP_SIMULATION;
      exit(EXIT_FAILURE);
    }
    friends[nof_friends++] = nodo_ricevuto;
    fprintf(ERR_FILE, "n%d: recieved SIGUSR2 from master. Added friend %d\n", getpid(), nodo_ricevuto);
    break;
  }
  /*gestione invio transazione periodico*/
  case SIGALRM:
  {
    if (nof_transaction > 0) {
      sigset_t mask;
      int chosen_friend;
      struct msg outcoming;
      outcoming.mtype = 1;

      bzero(&mask, sizeof(sigset_t));

      sigemptyset(&mask);
      sigaddset(&mask, SIGUSR1);
      sigprocmask(SIG_BLOCK, &mask, NULL);
      outcoming.mtext = transaction_pool[nof_transaction];
      chosen_friend = random_element(friends, nof_friends);
      if (chosen_friend == -1) {
        fprintf(LOG_FILE, "%s:%d: n%d: cannot choose a random friend\n", __FILE__, __LINE__, getpid());
        CHILD_STOP_SIMULATION;
        node_cleanup();
        exit(EXIT_FAILURE);
      }

      if (msgsnd(queue_id, &outcoming, sizeof(struct msg) - sizeof(long), IPC_NOWAIT) == -1) {
        if (errno != EAGAIN) {
          fprintf(ERR_FILE, "node n%d: SIGALRM, recieved an unexpected error while sending transaction to a friend: %s.\n", getpid(), strerror(errno));
          CHILD_STOP_SIMULATION;
          node_cleanup();
          exit(EXIT_FAILURE);
        }
      }
      else {
        nof_transaction--;
        kill(chosen_friend, SIGUSR1);
      }
      sigprocmask(SIG_BLOCK, &mask, NULL);
    }
    alarm(3);
    break;
  }
  case SIGSEGV:
    fprintf(ERR_FILE, "n%d: recieved a SIGSEGV, stopping simulation.\n", getpid());
    CHILD_STOP_SIMULATION;
    exit(EXIT_FAILURE);
    break;
  default:
    fprintf(LOG_FILE, "node %d: different signal received\n", getpid());
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
  act.sa_flags = SA_RESTART;
  act.sa_mask = mask;

  if (sigaction(SIGUSR1, &act, NULL) < 0) {
    fprintf(ERR_FILE, "init_node: could not associate handler to SIGUSR1.\n");
    node_cleanup();
    exit(EXIT_FAILURE);
  }
  if (sigaction(SIGUSR2, &act, NULL) < 0) {
    fprintf(ERR_FILE, "init_node: could not associate handler to SIGUSR2.\n");
    node_cleanup();
    exit(EXIT_FAILURE);
  }
  if (sigaction(SIGALRM, &act, NULL) < 0) {
    fprintf(ERR_FILE, "init_node: could not associate handler to SIGUSR1.\n");
    node_cleanup();
    exit(EXIT_FAILURE);
  }

  act.sa_flags = 0;
  if (sigaction(SIGTERM, &act, NULL) < 0) {
    fprintf(ERR_FILE, "init_node: could not associate handler to SIGTERM.\n");
    node_cleanup();
    exit(EXIT_FAILURE);
  }
  if (sigaction(SIGSEGV, &act, NULL) < 0) {
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

void simulate_processing(struct master_book book, int sem_id) {
  struct sembuf sops;
  sigset_t mask;
  int i = 0;
  int gain = 0;

  bzero(&sops, sizeof(struct sembuf));


  sleep_random_from_range(SO_MIN_TRANS_PROC_NSEC, SO_MAX_TRANS_PROC_NSEC);
  sops.sem_num = 1;
  sops.sem_op = -1;
  semop(sem_id, &sops, 1);

  while (i < SO_BLOCK_SIZE)
  {
    /* setta la posizione di memoria 0x0 a x*/
    book.blocks[i + *book.size] = transaction_pool[i];
    gain += transaction_pool[i].reward;
    i++;
  }
  /* transazione di guadagno del nodo */
  new_transaction(&book.blocks[i + *book.size], SELF_SENDER, getpid(), gain, 0);
  *book.size += SO_BLOCK_SIZE;

  sops.sem_num = 1;
  sops.sem_op = 1;
  semop(sem_id, &sops, 1);

  sigemptyset(&mask);
  sigaddset(&mask, SIGTERM);

  sigprocmask(SIG_BLOCK, &mask, NULL);
  get_out_of_the_pool();
  sigprocmask(SIG_UNBLOCK, NULL, &mask);

}

void init_node(int* friends_list, int pipe_read, int shm_book_id, int shm_book_size_id)
{
  struct sembuf sops;
  int sem_id;
  struct master_book book;

  bzero(&sops, sizeof(struct sembuf));

  friends = friends_list;
  nof_friends = SO_NUM_FRIENDS;

  if ((sem_id = semget(getppid(), 0, 0)) == -1) {
    fprintf(ERR_FILE, "node n%d: err\n", getpid());
    exit(EXIT_FAILURE);
  }

  generate();
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

  sops.sem_num = ID_READY_ALL;
  sops.sem_op = -1;
  semop(sem_id, &sops, 1);

  fprintf(LOG_FILE, "Node %d: completed setup. Queue id %d \n", getpid(), queue_id);

  sops.sem_op = 0;
  semop(sem_id, &sops, 1);

  while (1)
  {
    transaction* block = NULL;
    if (nof_transaction < SO_BLOCK_SIZE) {
      pause();
      continue;
    }
    alarm(3);
    simulate_processing(book, sem_id);

    free(block);
  }
}
