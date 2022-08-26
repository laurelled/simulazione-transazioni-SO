#include "../utils/utils.h"
#include "../master_book/master_book.h"
#include "../master/master.h"
#include "../ipc_functions/ipc_functions.h"

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

extern int SO_TP_SIZE;
extern int SO_NUM_FRIENDS;
extern int SO_MAX_TRANS_PROC_NSEC;
extern int SO_MIN_TRANS_PROC_NSEC;
extern int SO_HOPS;

/* IPC */
static int queue_id;
static int pipe_fd;

static int* friends;
static int nof_friends;

static transaction* transaction_pool;
volatile sig_atomic_t nof_transaction;

volatile sig_atomic_t alarm_flag;
volatile sig_atomic_t sigusr1_flag;
volatile sig_atomic_t sigusr2_flag;


static void cleanup() {
  free(transaction_pool);
  close(pipe_fd);
  msgctl(queue_id, IPC_RMID, NULL);
}

static void sig_handler(int sig) {
  struct msqid_ds stats;
  bzero(&stats, sizeof(struct msqid_ds));
  switch (sig) {

  case SIGTERM:
    /*segnale per la terminazione dell'esecuzione*/
    cleanup();
    exit(nof_transaction);
  case SIGUSR1:
  {
    struct msqid_ds stats;
    msgqnum_t msg_num;
    sigusr1_flag = 0;

    bzero(&stats, sizeof(struct msqid_ds));
    if (msgctl(queue_id, IPC_STAT, &stats) < 0) {
      fprintf(ERR_FILE, "node n%d sig_handler: reading error of IPC_STAT of queue with id %d. error %s.\n", getpid(), queue_id, strerror(errno));
      cleanup();
      exit(EXIT_FAILURE);
    }
    msg_num = stats.msg_qnum;
    /* per risolvere il merge dei segnali, il nodo legge tutti i messaggi presenti nella coda */
    while (msg_num-- > 0) {
      transaction t;
      struct msg incoming;

      if (msgrcv(queue_id, &incoming, sizeof(struct msg) - sizeof(long), 0, IPC_NOWAIT) == -1) {
        if (errno != ENOMSG) {
          fprintf(ERR_FILE, "sig_handler n%d: cannot read properly message. %s\n", getpid(), strerror(errno));
          cleanup();

          exit(EXIT_FAILURE);
        }
        break;
      }
      t = incoming.mtext;
      if (t.timestamp == 0) {
        fprintf(ERR_FILE, "n%d: ricevuta transazione vuota\n", getpid());
        print_transaction(t);
      }
      /*gestione invio transazione al master se SO_HOPS raggiunti */
      if (incoming.mtype == SO_HOPS + 1) {
        int master_q = 0;
        if ((master_q = msgget(getppid(), 0)) == -1) {
          fprintf(ERR_FILE, "node n%d: cannot connect to master message queue with key %d.\n", getpid(), getppid());
          cleanup();
          exit(EXIT_FAILURE);
        }
        incoming.mtype = getppid();
        if (msgsnd(master_q, &incoming, sizeof(struct msg) - sizeof(long), IPC_NOWAIT) == -1) {
          if (errno != EAGAIN) {
            fprintf(ERR_FILE, "node n%d: recieved an unexpected error while sending transaction to master: %s.\n", getpid(), strerror(errno));
            cleanup();

            exit(EXIT_FAILURE);
          }
          else {
            fprintf(ERR_FILE, "n%d: master queue full\n", getpid());
          }
        }
        else {
          kill(getppid(), SIGUSR1);
        }
      }
      /*gestione transaction_pool piena */
      else if (nof_transaction == SO_TP_SIZE) {
        int chosen_friend;
        int friend_queue = 0;
        chosen_friend = random_element(friends, nof_friends);
        if (chosen_friend == -1) {
          fprintf(LOG_FILE, "%s:%d: n%d: cannot choose a random friend\n", __FILE__, __LINE__, getpid());
          cleanup();
          exit(EXIT_FAILURE);
        }
        if ((friend_queue = msgget(chosen_friend, 0)) == -1) {
          fprintf(ERR_FILE, "node n%d: cannot connect to friend message queue with key %d (%s).\n", getpid(), chosen_friend, strerror(errno));
          cleanup();
          exit(EXIT_FAILURE);
        }
        incoming.mtype += 1;
        if (msgsnd(friend_queue, &incoming, sizeof(struct msg) - sizeof(long), IPC_NOWAIT) == -1) {
          if (errno != EAGAIN) {
            fprintf(ERR_FILE, "node n%d: recieved an unexpected error while sending transaction to a friend: %s.\n", getpid(), strerror(errno));
            cleanup();
            exit(EXIT_FAILURE);
          }
          else {
            int user_q = 0;
            if ((user_q = msgget(MSG_Q, 0)) == -1) {
              fprintf(ERR_FILE, "node n%d: cannot connect to master message queue with key %d.\n", getpid(), getppid());
              cleanup();
              exit(EXIT_FAILURE);
            }
            refuse_transaction(t, user_q);
          }
        }
        kill(chosen_friend, SIGUSR1);
      }
      /* accetta la transazione nella sua transaction pool*/
      else {
        transaction_pool[nof_transaction++] = t;
      }
    }
  }
  break;
  case SIGUSR2:
  {
    int nodo_ricevuto = 0;
    sigusr2_flag = 0;
    while (read(pipe_fd, &nodo_ricevuto, sizeof(int)) == -1) {
      if (errno != EINTR) {
        TEST_ERROR;
        cleanup();
        exit(EXIT_FAILURE);
      }
      errno = 0;
    }
    friends[nof_friends] = nodo_ricevuto;
    ++nof_friends;
  }
  break;
  case SIGALRM:
  {
    alarm_flag = 0;
    /*gestione invio transazione periodico*/
    if (nof_transaction > 0) {
      sigset_t mask;
      int chosen_friend;
      int friend_queue;
      struct msg outcoming;

      bzero(&mask, sizeof(sigset_t));

      outcoming.mtype = 1;
      outcoming.mtext = transaction_pool[nof_transaction - 1];
      chosen_friend = random_element(friends, nof_friends);
      if (chosen_friend == -1) {
        fprintf(LOG_FILE, "%s:%d: n%d: cannot choose a random friend\n", __FILE__, __LINE__, getpid());
        cleanup();
        exit(EXIT_FAILURE);
      }

      if ((friend_queue = msgget(chosen_friend, 0)) == -1) {
        fprintf(ERR_FILE, "node n%d: cannot connect to friend message queue with key %d (%s).\n", getpid(), chosen_friend, strerror(errno));
        cleanup();
        exit(EXIT_FAILURE);
      }

      if (msgsnd(friend_queue, &outcoming, sizeof(struct msg) - sizeof(long), IPC_NOWAIT) == -1) {
        if (errno != EAGAIN) {
          fprintf(ERR_FILE, "node n%d: SIGALRM, recieved an unexpected error while sending transaction to a friend: %s.\n", getpid(), strerror(errno));

          cleanup();
          exit(EXIT_FAILURE);
        }
      }
      else {
        --nof_transaction;
        kill(chosen_friend, SIGUSR1);
      }
    }
  }
  alarm(1);
  break;
  case SIGSEGV:
    fprintf(ERR_FILE, "n%d: recieved a SIGSEGV, stopping simulation.\n", getpid());
    cleanup();
    exit(EXIT_FAILURE);
    break;
  default:
    fprintf(LOG_FILE, "node %d: different signal received\n", getpid());
  }

}

static void generate() {
  sigset_t mask;
  struct sigaction act;
  struct msqid_ds stats;


  /* system V message queue */
  if ((queue_id = msgget(getpid(), IPC_CREAT | IPC_EXCL | S_IWUSR | S_IRUSR)) < 0) {
    fprintf(ERR_FILE, "init_node: message queue already exists. Check ipcs and remove it with iprm -Q %d.\n", getpid());
    cleanup();
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
    cleanup();
    CHILD_STOP_SIMULATION;
    exit(EXIT_FAILURE);
  }

  /* signal handling init */
  bzero(&mask, sizeof(sigset_t));
  sigemptyset(&mask);
  sigaddset(&mask, SIGALRM);
  sigaddset(&mask, SIGUSR2);
  act.sa_handler = sig_handler;
  act.sa_flags = 0;
  act.sa_mask = mask;

  if (sigaction(SIGUSR1, &act, NULL) < 0) {
    fprintf(ERR_FILE, "init_node: could not associate handler to SIGUSR1.\n");
    cleanup();
    exit(EXIT_FAILURE);
  }

  sigemptyset(&mask);
  sigaddset(&mask, SIGUSR1);
  sigaddset(&mask, SIGALRM);
  act.sa_mask = mask;
  if (sigaction(SIGUSR2, &act, NULL) < 0) {
    fprintf(ERR_FILE, "init_node: could not associate handler to SIGUSR2.\n");
    cleanup();
    exit(EXIT_FAILURE);
  }

  sigaddset(&mask, SIGUSR1);
  sigaddset(&mask, SIGUSR2);
  act.sa_mask = mask;
  if (sigaction(SIGALRM, &act, NULL) < 0) {
    fprintf(ERR_FILE, "init_node: could not associate handler to SIGUSR1.\n");
    cleanup();
    exit(EXIT_FAILURE);
  }

  sigemptyset(&mask);
  act.sa_mask = mask;
  act.sa_flags = 0;
  if (sigaction(SIGTERM, &act, NULL) < 0) {
    fprintf(ERR_FILE, "init_node: could not associate handler to SIGTERM.\n");
    cleanup();
    exit(EXIT_FAILURE);
  }
  if (sigaction(SIGSEGV, &act, NULL) < 0) {
    fprintf(ERR_FILE, "init_node: could not associate handler to SIGTERM.\n");
    cleanup();
    exit(EXIT_FAILURE);
  }
}

void simulate_processing(struct master_book book, int sem_id) {
  struct sembuf sops;
  sigset_t mask;

  bzero(&sops, sizeof(struct sembuf));
  bzero(&mask, sizeof(sigset_t));

  sigemptyset(&mask);
  sigaddset(&mask, SIGALRM);
  sigaddset(&mask, SIGUSR2);
  sigaddset(&mask, SIGUSR1);
  sigprocmask(SIG_BLOCK, &mask, NULL);

  sleep_random_from_range(SO_MIN_TRANS_PROC_NSEC, SO_MAX_TRANS_PROC_NSEC);
  /* ID_MEM ottenuto tramite master.h */
  sops.sem_num = ID_MEM;
  sops.sem_op = -1;
  semop(sem_id, &sops, 1);

  {
    int i = 0, gain = 0, block_i, size = *(book.size);
    if (size < SO_REGISTRY_SIZE) {
      transaction node_gain;

      block_i = size * SO_BLOCK_SIZE;
      while (i < SO_BLOCK_SIZE - 1)
      {
        book.blocks[i + block_i] = transaction_pool[i];
        gain += transaction_pool[i].reward;
        i++;
      }

      /* transazione di guadagno del nodo */
      new_transaction(&node_gain, SELF_SENDER, getpid(), gain, 0);
      book.blocks[block_i + SO_BLOCK_SIZE - 1] = node_gain;
      (*book.size)++;
      memmove(transaction_pool, transaction_pool + SO_BLOCK_SIZE - 1, sizeof(transaction) * (nof_transaction + 1 - SO_BLOCK_SIZE));
      nof_transaction -= (SO_BLOCK_SIZE - 1);
    }
    else {
      fprintf(ERR_FILE, "[%ld] n%d: master book maximum capacity reached, notifying master.\n", clock() / CLOCKS_PER_SEC, getpid());
      kill(getppid(), SIGUSR2);
    }
  }

  sops.sem_op = 1;
  while (semop(sem_id, &sops, 1) == -1) {
    if (errno != EINTR) {
      TEST_ERROR;
      cleanup();
      exit(EXIT_FAILURE);
    }
    errno = 0;
  }

  sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

void init_node(int* friends_list, int pipe_read, int shm_book_id, int shm_book_size_id)
{
  int ppid = getppid();
  struct sembuf sops;
  int sem_id;
  struct master_book book;

  bzero(&sops, sizeof(struct sembuf));
  bzero(&book, sizeof(struct master_book));

  friends = friends_list;
  nof_friends = SO_NUM_FRIENDS;
  pipe_fd = pipe_read;

  if ((sem_id = semget(getppid(), 0, S_IRUSR | S_IWUSR)) == -1) {
    fprintf(ERR_FILE, "node n%d: error retrieving parent semaphore (%s)\n", getpid(), strerror(errno));
    exit(EXIT_FAILURE);
  }

  generate();

  if ((book.blocks = shmat(shm_book_id, NULL, 0)) == NULL) {
    fprintf(ERR_FILE, "node: the process cannot be attached to the array shared memory.\n");
    cleanup();
    exit(EXIT_FAILURE);
  }

  if ((book.size = shmat(shm_book_size_id, NULL, 0)) == NULL) {
    fprintf(ERR_FILE, "node: the process cannot be attached to the array shared memory.\n");
    cleanup();
    exit(EXIT_FAILURE);
  }

  /* ID_READY_ALL ottenuto da master.h */
  sops.sem_num = ID_READY_ALL;
  sops.sem_op = -1;
  while (semop(sem_id, &sops, 1) == -1) {
    if (errno != EINTR) {
      TEST_ERROR;
      cleanup();
      exit(EXIT_FAILURE);
    }
    errno = 0;
  }

  sops.sem_op = 0;
  while (semop(sem_id, &sops, 1) == -1) {
    if (errno != EINTR) {
      TEST_ERROR;
      cleanup();
      exit(EXIT_FAILURE);
    }
    errno = 0;
  }

  alarm(1);
  while (kill(ppid, 0) != -1 && errno != ESRCH)
  {
    if (nof_transaction < SO_BLOCK_SIZE - 1) {
      pause();
      continue;
    }
    simulate_processing(book, sem_id);
  }

  fprintf(ERR_FILE, "n%d: master (%d) terminated. Ending...\n", getpid(), ppid);
  exit(EXIT_FAILURE);
}
