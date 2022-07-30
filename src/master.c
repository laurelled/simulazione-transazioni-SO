#include "utils/utils.h"
#include "load_constants/load_constants.h"
#include "pid_list/pid_list.h"
#include "transaction.h"
#include "node/node.h"
#include "user/user.h"

#include <sys/time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define SIM_END_SEC 0
#define SIM_END_USR 1
#define SIM_END_SIZ 2

#define EARLY_FAILURE 69

#define MAX_USERS_TO_PRINT 10
#define SO_REGISTRY_SIZE 10

#define ID_READY 0
#define NUM_SEM 2

#define SHM_SIZE sizeof(transaction) * SO_BLOCK_SIZE * SO_REGISTRY_SIZE

struct master_book {
  unsigned int cursor;
  transaction** blocks;
};

struct master_book* book;

extern int SO_USERS_NUM;
extern int SO_NODES_NUM;
extern int SO_SIM_SEC;

static int inactive_users;
static int inactive_nodes;
static int simulation_seconds;
static int stop_sim = 0;

static int shm_id;

void handler(int signal) {
  switch (signal) {
  case SIGALRM:
    fprintf(LOG_FILE, "NUMBER OF ACTIVE USERS %d | NUMBER OF ACTIVE NODES %d", SO_USERS_NUM - inactive_users, SO_NODES_NUM - inactive_nodes);
    /* il budget corrente di ogni processo utente e di ogni processo nodo, cosı̀ come registrato nel libro mastro
    (inclusi i processi utente terminati). Oppure quelli con maggior e minor budget. */
    simulation_seconds++;
    alarm(1);
    break;
  case SIGTERM:
    stop_sim = 1;
    fprintf(LOG_FILE, "master: recieved a SIGTERM at %ds from simulation start\n", simulation_seconds);
    break;
  }
}

void stop_simulation(pid_t* users, pid_t* nodes) {
  int i = 0;
  while (users[i] != 0 && i < SO_USERS_NUM) {
    kill(users[i++], SIGTERM);
  }
  i = 0;
  while (nodes[i] != 0 && i < SO_NODES_NUM) {
    kill(nodes[i++], SIGTERM);
  }
}

void cleanup(pid_t* users, pid_t* nodes) {
  if (users != NULL || nodes != NULL)
    stop_simulation(users, nodes);

  if (shmctl(shm_id, 0, IPC_RMID) == -1) {
    fprintf(ERR_FILE, "%s cleanup: cannot remove shm with id %d\n", __FILE__, shm_id);
  }
}

int  start_shared_memory() {
  return shmget(getpid(), SHM_SIZE, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
}

struct master_book* get_master_book(int shm_id) {
  struct master_book* new = NULL;
  transaction** ptr;
  if ((ptr = shmat(shm_id, NULL, 0)) == (void*)-1) {
    fprintf(ERR_FILE, "master: the process can't be attached\n");
    return NULL;
  }
  new = malloc(sizeof(struct master_book));
  new->cursor = 0;
  new->blocks = ptr;

  return new;
}

void periodical_print() {
  fprintf(LOG_FILE, "ACTIVE USERS: %d | ACTIVE NODES: %d\n\n", SO_USERS_NUM, SO_NODES_NUM);
  if (SO_NODES_NUM < MAX_USERS_TO_PRINT) {

  }
  else {

  }
}

void summary_print(int reason, int* nof_transactions, pid_t* nodes) {
  int i = 0;
  /* TODO: bilancio di ogni processo utente, compresi quelli che sono terminati prematuramente */
  /* TODO: bilancio di ogni processo nodo */
  fprintf(LOG_FILE, "NUMBER OF INACTIVE USERS: %d\n", inactive_users);
  fprintf(LOG_FILE, "NUMBER OF TRANSACTION BLOCK WRITTEN INTO THE MASTER BOOK: %d\n", book->cursor);

  fprintf(LOG_FILE, "\n\n");

  fprintf(LOG_FILE, "Number of transactions left per node:\n");
  while (i < SO_NODES_NUM) {
    fprintf(LOG_FILE, "NODE %d: %d transactions left\n", nodes[i], nof_transactions[i]);
    i++;
  }
}

int main() {
  sigset_t mask;
  struct sigaction act;
  pid_t* users = init_list(SO_USERS_NUM);
  pid_t* nodes = init_list(SO_NODES_NUM);

  int i = 0;
  struct sembuf sops;
  int sem_id;

  bzero(&mask, sizeof(sigset_t));
  sigemptyset(&mask);
  act.sa_handler = handler;
  act.sa_flags = 0;
  act.sa_mask = mask;

  if (sigaction(SIGALRM, &act, NULL) < 0) {
    fprintf(ERR_FILE, "master: could not associate handler to SIGALRM.\n");
    exit(EXIT_FAILURE);
  }
  if (sigaction(SIGTERM, &act, NULL) < 0) {
    fprintf(ERR_FILE, "master: could not associate handler to SIGTERM.\n");
    exit(EXIT_FAILURE);
  }

  if ((shm_id = start_shared_memory()) == -1) {
    fprintf(ERR_FILE, "master: cannot start shared memory. Please clear the shm with id %d\n", getpid());
    exit(EXIT_FAILURE);
  }
  if ((book = get_master_book(shm_id)) == NULL) {
    fprintf(ERR_FILE, "master: the process cannot be attached to the shared memory.\n");
    cleanup(NULL, NULL);
    exit(EXIT_FAILURE);
  }

  if ((sem_id = semget(getpid(), NUM_SEM, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR)) == -1) {
    fprintf(ERR_FILE, "master: semaphore %d already exists.\n", getpid());
    exit(EXIT_FAILURE);
  }



  init_master();

  while (i < SO_USERS_NUM) {
    pid_t child;

    switch ((child = fork())) {
    case -1:
      fprintf(ERR_FILE, "master: fork failed for user creation.\n");
      cleanup(users, nodes);
      exit(EXIT_FAILURE);
    case 0:
    {
      struct sembuf sops;
      sops.sem_num = ID_READY;
      sops.sem_op = -1;
      semop(sem_id, &sops, 1);

      sops.sem_op = 0;
      semop(sem_id, &sops, 1);

      init_user(users, nodes);
      break;
    }
    default:
      users[i++] = child;
      break;
    }
  }

  i = 0;
  while (i < SO_NODES_NUM) {
    pid_t child;
    switch ((child = fork())) {
    case -1:
      fprintf(ERR_FILE, "master: fork failed for node creation.\n");
      cleanup(users, nodes);
      exit(EXIT_FAILURE);
    case 0:
      generate_node();
      break;
    default:
      nodes[i++] = child;
      break;
    }
  }

  if (shmctl(shm_id, 0, IPC_RMID) == -1) {
    fprintf(ERR_FILE, "master: error in shmctl while removing a shm\n");
    exit(EXIT_FAILURE);
  }



  /* master gives "green light" to all user processes */
  sops.sem_num = ID_READY;
  sops.sem_op = -1;
  semop(sem_id, &sops, 1);

  sops.sem_num = ID_READY;
  sops.sem_op = 0;
  semop(sem_id, &sops, 1);

  {
    i = 0;
    int* nof_transactions[SO_NODES_NUM];
    while (!stop_sim && simulation_seconds < SO_SIM_SEC && inactive_users < SO_USERS_NUM && book->cursor < SO_REGISTRY_SIZE) {
      int status = 0;
      int node_index = 0;
      int terminated_p = wait(&status);
      if (terminated_p == -1) {
        if (errno == EINTR) {
          continue;
        }
        exit(EXIT_FAILURE);
      }
      if ((node_index = find_element(nodes, SO_NODES_NUM, terminated_p)) != -1) {
        inactive_nodes++;
        if (WIFEXITED(status)) {
          if (WEXITSTATUS(status) == EXIT_FAILURE) {
            fprintf(ERR_FILE, "Node %d encountered an error. Continuing simulation...\n", terminated_p);
            nof_transactions[node_index] = -1;
          }
          else {
            fprintf(ERR_FILE, "master: node %d terminated with the unexpected status %d while sim was ongoing. Check the code.\n", terminated_p, WEXITSTATUS(status));
            cleanup(users, nodes);
            exit(EXIT_FAILURE);
          }
        }
      }
      else {
        inactive_users++;
      }
    }
    stop_simulation(users, nodes);

    while (1) {
      int status;
      int index;
      int terminated_p = wait(&status);
      if (terminated_p == -1) {
        if (errno == EINTR) {
          continue;
        }
        else if (errno == ECHILD) {
          break;
        }
        exit(EXIT_FAILURE);
      }
      if ((index = find_element(nodes, SO_NODES_NUM, terminated_p)) != -1) {
        if (WIFEXITED(status)) {
          int exit_status = 0;
          switch (exit_status = WEXITSTATUS(status)) {
          case EXIT_FAILURE:
            nof_transactions[index] = -1;
            break;
          default:
            nof_transactions[index] = exit_status;
            break;
          }
        }
      }
    }

    int reason = SIM_END_SEC;
    if (SO_USERS_NUM == 0) {
      reason = SIM_END_USR;
    }
    else if (book->cursor == SO_REGISTRY_SIZE) {
      reason = SIM_END_SIZ;
    }
    summary_print(reason, nof_transactions, nodes);
  }


  free_list(users);
  free_list(nodes);
  exit(EXIT_SUCCESS);
}
