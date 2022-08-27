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

#define ALARM_PERIOD 1

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
  if (transaction_pool != NULL)
    free(transaction_pool);
  if (friends != NULL)
    free(friends);

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
    ++sigusr1_flag;
    break;
  case SIGUSR2:
    ++sigusr2_flag;
    break;
  case SIGALRM:
    ++alarm_flag;
    alarm(ALARM_PERIOD);
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
  /* system V message queue */
  queue_id = msgget(getpid(), IPC_CREATION_FLAGS);
  TEST_ERROR_AND_FAIL;

  /* transaction pool init */
  transaction_pool = (transaction*)malloc(SO_TP_SIZE * sizeof(transaction));
  TEST_ERROR_AND_FAIL;


  if (SO_BLOCK_SIZE >= SO_TP_SIZE) {
    fprintf(LOG_FILE, "init_node: SO_BLOCK_SIZE >= SO_TP_SIZE. Check environmental config.\n");
    cleanup();
    CHILD_STOP_SIMULATION;
    exit(EXIT_FAILURE);
  }

  {
    sigset_t mask;
    struct sigaction act;
    struct msqid_ds stats;

    bzero(&mask, sizeof(sigset_t));
    /* signal handling*/
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    sigaddset(&mask, SIGUSR2);
    act.sa_handler = sig_handler;
    act.sa_flags = SA_NODEFER;
    act.sa_mask = mask;

    sigaction(SIGUSR1, &act, NULL);
    TEST_ERROR_AND_FAIL;

    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGALRM);
    act.sa_mask = mask;
    sigaction(SIGUSR2, &act, NULL);
    TEST_ERROR_AND_FAIL;

    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    act.sa_mask = mask;
    sigaction(SIGALRM, &act, NULL);
    TEST_ERROR_AND_FAIL;

    sigfillset(&mask);
    act.sa_mask = mask;
    act.sa_flags = 0;
    sigaction(SIGTERM, &act, NULL);
    TEST_ERROR_AND_FAIL;
    sigaction(SIGSEGV, &act, NULL);
    TEST_ERROR_AND_FAIL;
  }
}

void simulate_processing(struct master_book book, int sem_id) {
  sleep_random_from_range(SO_MIN_TRANS_PROC_NSEC, SO_MAX_TRANS_PROC_NSEC);

  /* ID_MEM ottenuto tramite master.h */
  sem_reserve(sem_id, ID_MEM);

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

      /* node transaction */
      new_transaction(&node_gain, SELF_SENDER, getpid(), gain, 0);
      book.blocks[block_i + SO_BLOCK_SIZE - 1] = node_gain;
      (*book.size)++;

      /* shift transaction_pool by SO_BLOCK_SIZE - 1 */
      memmove(transaction_pool, transaction_pool + SO_BLOCK_SIZE - 1, sizeof(transaction) * (nof_transaction + 1 - SO_BLOCK_SIZE));
      nof_transaction -= (SO_BLOCK_SIZE - 1);
    }
    else {
      /* master book maximum capacity reached, the node send a SIGUSR2 to master to stop simulation */
      kill(getppid(), SIGUSR2);
    }
  }

  sem_release(sem_id, ID_MEM);
}

void init_node(int* friends_list, int pipe_read, int shm_book_id, int shm_book_size_id)
{
  int ppid = getppid();
  int sem_id;
  struct master_book book;

  bzero(&book, sizeof(struct master_book));

  friends = friends_list;
  nof_friends = SO_NUM_FRIENDS;
  pipe_fd = pipe_read;

  sem_id = semget(getppid(), 0, S_IRUSR | S_IWUSR);
  TEST_ERROR_AND_FAIL;

  generate();

  book.blocks = shmat(shm_book_id, NULL, 0);
  TEST_ERROR_AND_FAIL;
  book.size = shmat(shm_book_size_id, NULL, 0);
  TEST_ERROR_AND_FAIL;

  /* ID_READY_ALL ottenuto da master.h */
  sem_reserve(sem_id, ID_READY_ALL);
  TEST_ERROR_AND_FAIL;
  sem_wait_for_zero(sem_id, ID_READY_ALL);

  alarm(ALARM_PERIOD);
  while (kill(ppid, 0) != -1 && errno != ESRCH)
  {
    if (sigusr1_flag) {
      msgqnum_t msg_num;
      sigusr1_flag = 0;
      errno = 0;
      {
        struct msqid_ds stats;
        bzero(&stats, sizeof(struct msqid_ds));
        msgctl(queue_id, IPC_STAT, &stats);
        TEST_ERROR_AND_FAIL;
        msg_num = stats.msg_qnum;
      }
      /* per risolvere il merge dei segnali, il nodo legge tutti i messaggi presenti nella coda */
      while (msg_num-- > 0) {
        transaction incoming_t;
        struct msg incoming;

        if (msgrcv(queue_id, &incoming, sizeof(struct msg) - sizeof(long), 0, IPC_NOWAIT) == -1) {
          if (errno != ENOMSG) {
            TEST_ERROR_AND_FAIL;
          }
          /* errno = ENOMSG -> no message left in the queue, breaking from while. */
          break;
        }
        incoming_t = incoming.mtext;

        /*gestione invio transazione al master se SO_HOPS raggiunti */
        if (incoming.mtype == SO_HOPS + 1) {
          int master_q = 0;
          master_q = msgget(getppid(), 0);
          TEST_ERROR_AND_FAIL;
          incoming.mtype = 1;
          msgsnd(master_q, &incoming, sizeof(struct msg) - sizeof(long), IPC_NOWAIT);
          TEST_ERROR_AND_FAIL;

          /* master queue was full, refusing user transaction */
          if (errno == EAGAIN) {
            int user_q = msgget(MSG_Q, 0);
            TEST_ERROR_AND_FAIL;

            refuse_transaction(incoming_t, user_q);
            TEST_ERROR_AND_FAIL;
          }
          /* transaction was successful sent to master queue, sending signal to master */
          else {
            kill(getppid(), SIGUSR1);
          }
        }

        /*gestione transaction_pool piena */
        else if (nof_transaction == SO_TP_SIZE) {
          int chosen_friend, friend_queue = 0;
          chosen_friend = random_element(friends, nof_friends);
          if (chosen_friend == -1) {
            fprintf(LOG_FILE, "%s:%d: n%d: cannot choose a random friend\n", __FILE__, __LINE__, getpid());
            cleanup();
            exit(EXIT_FAILURE);
          }
          friend_queue = msgget(chosen_friend, 0);
          TEST_ERROR_AND_FAIL;

          incoming.mtype += 1;
          if (msgsnd(friend_queue, &incoming, sizeof(struct msg) - sizeof(long), IPC_NOWAIT) == -1) {
            if (errno != EAGAIN) {
              fprintf(ERR_FILE, "node n%d: recieved an unexpected error while sending transaction to a friend: %s.\n", getpid(), strerror(errno));
              cleanup();
              exit(EXIT_FAILURE);
            }
            else {
              int user_q = msgget(MSG_Q, 0);
              TEST_ERROR_AND_FAIL;

              refuse_transaction(incoming_t, user_q);
              TEST_ERROR_AND_FAIL;
            }
          }
          else {
            kill(chosen_friend, SIGUSR1);
          }
        }
        /* accetta la transazione nella sua transaction pool*/
        else {
          transaction_pool[nof_transaction++] = incoming_t;
        }
      }
    }
    if (alarm_flag) {
      alarm_flag = 0;
      errno = 0;
      /*gestione invio transazione periodico */
      if (nof_transaction > 0) {
        int chosen_friend;
        int friend_queue;
        struct msg outcoming;

        outcoming.mtype = 1;
        outcoming.mtext = transaction_pool[nof_transaction - 1];
        chosen_friend = random_element(friends, nof_friends);
        if (chosen_friend == -1) {
          fprintf(LOG_FILE, "%s:%d: n%d: cannot choose a random friend\n", __FILE__, __LINE__, getpid());
          cleanup();
          exit(EXIT_FAILURE);
        }

        friend_queue = msgget(chosen_friend, 0);
        TEST_ERROR_AND_FAIL;
        msgsnd(friend_queue, &outcoming, sizeof(struct msg) - sizeof(long), IPC_NOWAIT);
        TEST_ERROR_AND_FAIL;
        if (errno != EAGAIN) {
          --nof_transaction;
          kill(chosen_friend, SIGUSR1);
        }
      }
    }
    if (sigusr2_flag) {
      int nodo_ricevuto = 0;
      errno = 0;
      sigusr2_flag = 0;
      while (read(pipe_fd, &nodo_ricevuto, sizeof(int)) == -1) {
        if (errno != EINTR) {
          TEST_ERROR_AND_FAIL;
        }
        errno = 0;
      }
      friends[nof_friends] = nodo_ricevuto;
      ++nof_friends;
    }

    if (nof_transaction < SO_BLOCK_SIZE - 1) {
      pause();
      continue;
    }
    simulate_processing(book, sem_id);
  }
  fprintf(ERR_FILE, "n%d: master (%d) terminated. Ending...\n", getpid(), ppid);
  exit(EXIT_FAILURE);
}
