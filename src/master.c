#include "utils/utils.h"
#include "load_constants/load_constants.h"
#include "pid_list/pid_list.h"
#include "master_book/master_book.h"
#include "node/node.h"
#include "user/user.h"

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

union semun {
  int val;
  struct semid_ds* buf;
  unsigned short* array;
#if defined(__linux__)
  struct seminfo* __buf;
#endif
};

#define SIM_END_SEC 0
#define SIM_END_USR 1
#define SIM_END_SIZ 2

#define NUM_PROC SO_NODES_NUM + SO_USERS_NUM + 1
#define EARLY_FAILURE 69

#define MAX_USERS_TO_PRINT 10
#define SO_REGISTRY_SIZE 10

#define ID_READY 0
#define ID_MEM 1 /*TODO semaforo per accesso scrittura alla memoria*/
#define NUM_SEM 2

#define REGISTRY_SIZE sizeof(transaction) * SO_BLOCK_SIZE * SO_REGISTRY_SIZE

struct master_book* book;

extern int SO_USERS_NUM;
extern int SO_NODES_NUM;
extern int SO_SIM_SEC;
extern int SO_NUM_FRIENDS;

static pid_t* users;
static pid_t* nodes;

static int* user_budget;
static int* node_budget;
static int block_reached;

static int inactive_users;
static int nof_nodes;
static int simulation_seconds;
static int stop_sim = 0;

void periodical_update(pid_t* users, int* user_budget, int* node_budget) {
  int i = block_reached;
  unsigned int size = book->cursor;
  while (i < size) {
    transaction* ptr = book->blocks[i++];
    int j = 0;
    int index;
    while (j++ < SO_BLOCK_SIZE - 1) {
      int index;
      if ((index = find_element(users, SO_USERS_NUM, ptr->sender)) != -1) {
        user_budget[index] -= (ptr->quantita + ptr->reward);
        index = find_element(users, SO_USERS_NUM, ptr->receiver);
        user_budget[index] += ptr->quantita;
      }
      ptr++;
    }
    index = find_element(nodes, SO_NODES_NUM, ptr->receiver);
    node_budget[index] += ptr->quantita;
  }
  block_reached = i;
}

void stop_simulation(pid_t* users, pid_t* nodes) {
  int i = 0;
  pid_t child;
  while ((child = users[i]) != 0 && i < SO_USERS_NUM) {
    if (kill(child, SIGTERM) == -1) {
      if (errno == ESRCH)
        continue;

      fprintf(ERR_FILE, "stop_simulation: encountered an unexpected error while trying to kill user %d\n", child);
      exit(EXIT_FAILURE);
    }
  }
  i = 0;
  while (child = nodes[i] != 0 && i < SO_NODES_NUM) {
    if (kill(nodes[i++], SIGTERM) == -1 && errno == ESRCH) {
      continue;
    }
    fprintf(ERR_FILE, "stop simulation: encountered an unexpected error while trying to kill node %d\n", child);
    exit(EXIT_FAILURE);
  }
}

void cleanup(pid_t* users, pid_t* nodes, int shm_id, int sem_id) {
  stop_simulation(users, nodes);

  free_list(users);
  free_list(nodes);

  if (shm_id != -1) {
    if (shmctl(shm_id, 0, IPC_RMID) == -1) {
      fprintf(ERR_FILE, "%s cleanup: cannot remove shm with id %d\n. Please check ipcs and remove it.\n", __FILE__, shm_id);
    }
  }

  if (sem_id != -1) {
    if (semctl(sem_id, 0, IPC_RMID) == -1) {
      fprintf(ERR_FILE, "master: could not free sem %d, please check ipcs and remove it.\n", sem_id);
    }
  }
}

int  start_shared_memory() {
  return shmget(getpid(), sizeof(struct master_book), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
}

void periodical_print(pid_t* users, int* user_budget, int* node_budget) {
  int i = 0;
  fprintf(LOG_FILE, "NUMBER OF ACTIVE USERS %d | NUMBER OF ACTIVE NODES %d", SO_USERS_NUM - inactive_users, SO_NODES_NUM);
  /* budget di ogni processo utente (inclusi quelli terminati prematuramente)*/
  while (i < SO_USERS_NUM) {
    fprintf(LOG_FILE, "CHILD PID%d : %d\n", users[i], user_budget[i++]);
  }
  i = 0;
  /* budget di ogni processo nodo */
  while (i < SO_NODES_NUM) {
    fprintf(LOG_FILE, "NODE PID%d : %d\n", nodes[i], node_budget[i++]);
  }
}

void summary_print(int ending_reason, int* nof_transactions, pid_t* nodes) {
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

void wait_siblings(int sem_id, int sem_num) {
  struct sembuf sops;
  pid_t* friends;
  sops.sem_num = sem_num;
  sops.sem_op = -1;
  semop(sem_id, &sops, 1);

  sops.sem_op = 0;
  semop(sem_id, &sops, 1);
}

pid_t* assign_friends(pid_t* nodes) {
  pid_t* friends = init_list(SO_NUM_FRIENDS);
  int i = 0;

  while (i < SO_NUM_FRIENDS) {
    pid_t random_el = random_element(nodes, SO_NODES_NUM);
    if (random_el == -1) {
      fprintf(ERR_FILE, "assign_friends: something went wrong with the extraction of friends\n");
      free(friends);
      return NULL;
    }
    /* se trova un pid già inserito, estrae un altro pid */
    if (find_element(friends, SO_NUM_FRIENDS, random_el) != -1)
      continue;
    friends[i++] = random_el;
  }

  return friends;
}

void handler(int signal) {
  switch (signal) {
  case SIGALRM:
    periodical_update(users, user_budget, node_budget);
    periodical_print(users, user_budget, node_budget);
    simulation_seconds++;
    alarm(1);
    break;
  case SIGINT:
    stop_sim = 1;
    fprintf(LOG_FILE, "master: recieved a SIGTERM at %ds from simulation start\n", simulation_seconds);
    break;
  case SIGUSR1:
  {

    pid_t child;
    switch ((child = fork())) {
    case -1:
      fprintf(ERR_FILE, "master handler: fork failed for node creation.\n");
      /*cleanup(users, nodes, shm_id, sem_id); TODO : parametri da vedere*/
      exit(EXIT_FAILURE);
      break;
    case 0:
    {
      pid_t* friends;
      friends = assign_friends(nodes);
      init_node(friends);
      break;
    }
    default:
      nodes[nof_nodes++] = child;
      break;
    }

    break;
  }
  }
}

int main() {
  sigset_t mask;
  struct sigaction act;
  struct sembuf sops;
  int shm_id = -1;
  int sem_id = -1;
  users = init_list(SO_USERS_NUM);
  nodes = init_list(SO_NODES_NUM);
  nof_nodes = SO_NODES_NUM;

  fprintf(LOG_FILE, "[!] MASTER PID: %d\n", getpid());

  bzero(&mask, sizeof(sigset_t));
  sigemptyset(&mask);
  act.sa_handler = handler;
  act.sa_flags = 0;
  act.sa_mask = mask;

  if (sigaction(SIGALRM, &act, NULL) < 0) {
    fprintf(ERR_FILE, "master: could not associate handler to SIGALRM.\n");
    cleanup(users, nodes, shm_id, sem_id);
    exit(EXIT_FAILURE);
  }
  if (sigaction(SIGINT, &act, NULL) < 0) {
    fprintf(ERR_FILE, "master: could not associate handler to SIGTERM.\n");
    cleanup(users, nodes, shm_id, sem_id);
    exit(EXIT_FAILURE);
  }

  if ((sem_id = semget(getpid(), NUM_SEM, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR)) == -1) {
    fprintf(ERR_FILE, "master: semaphore %d already exists.\n", getpid());
    free_list(users);
    free_list(nodes);
    exit(EXIT_FAILURE);
  }

  if (semctl(sem_id, ID_READY, SETVAL, NUM_PROC) == -1) {
    fprintf(ERR_FILE, "master: cannot set initial value for sem ID_READY with id %d.\n", sem_id);
    cleanup(users, nodes, shm_id, sem_id);
    exit(EXIT_FAILURE);
  }

  if (semctl(sem_id, ID_MEM, SETVAL, 1) == -1) {
    fprintf(ERR_FILE, "master: cannot set initial value for sem ID_MEM with id %d.\n", sem_id);
    cleanup(users, nodes, shm_id, sem_id);
    exit(EXIT_FAILURE);
  }

  if ((shm_id = start_shared_memory()) == -1) {
    fprintf(ERR_FILE, "master: cannot start shared memory. Please clear the shm with id %d\n", getpid());
    cleanup(users, nodes, shm_id, sem_id);
    exit(EXIT_FAILURE);
  }
  if ((book = get_master_book(shm_id)) == NULL) {
    fprintf(ERR_FILE, "master: the process cannot be attached to the shared memory.\n");
    cleanup(users, nodes, shm_id, sem_id);
    exit(EXIT_FAILURE);
  }
  book->blocks = malloc(REGISTRY_SIZE);
  book->cursor = 0;

  {
    int i = 0;
    while (i < SO_USERS_NUM) {
      pid_t child;

      switch ((child = fork())) {
      case -1:
        fprintf(ERR_FILE, "master: fork failed for user creation iteration %d/%d.\n", i + 1, SO_USERS_NUM);
        cleanup(users, nodes, shm_id, sem_id);
        exit(EXIT_FAILURE);
      case 0:
      {
        wait_siblings(sem_id, ID_READY);
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
        fprintf(ERR_FILE, "master: fork failed for node creation iteration %d/%d.\n", i + 1, SO_NODES_NUM);
        cleanup(users, nodes, shm_id, sem_id);
        exit(EXIT_FAILURE);
      case 0:
      {
        pid_t* friends;
        wait_siblings(sem_id, ID_READY);
        friends = assign_friends(nodes);
        init_node(friends);
        break;
      }
      default:
        nodes[i++] = child;
        break;
      }
    }
  }

  /* master gives "green light" to all child processes */
  sops.sem_num = ID_READY;
  sops.sem_op = -1;
  semop(sem_id, &sops, 1);

  /*test per debug*/
  sops.sem_num = ID_READY;
  sops.sem_op = 0;
  semop(sem_id, &sops, 1);

  if (shmctl(shm_id, 0, IPC_RMID) == -1) {
    fprintf(ERR_FILE, "master: error in shmctl while removing the shm with id %d. Please check ipcs and remove it.\n", shm_id);
    exit(EXIT_FAILURE);
  }

  {
    int* nof_transactions = malloc(sizeof(int) * SO_NODES_NUM);
    int ending_reason;

    int i = 0;
    while (!stop_sim && simulation_seconds < SO_SIM_SEC && inactive_users < SO_USERS_NUM && book->cursor < SO_REGISTRY_SIZE) {
      int status = 0;
      int node_index = 0;
      int terminated_p = wait(&status);
      if (terminated_p == -1) {
        if (errno == EINTR) {
          continue;
        }
        cleanup(users, nodes, shm_id, sem_id);
        fprintf(ERR_FILE, "master: wait failed for an unexpected error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
      }
      if ((node_index = find_element(nodes, SO_NODES_NUM, terminated_p)) != -1) {
        if (WIFEXITED(status)) {
          if (WEXITSTATUS(status) == EXIT_FAILURE) {
            fprintf(ERR_FILE, "Node %d encountered an error. Stopping simulation\n", terminated_p);
            cleanup(users, nodes, shm_id, sem_id);
            free(nof_transactions);
            exit(EXIT_FAILURE);
          }
          else {
            fprintf(ERR_FILE, "master: node %d terminated with the unexpected status %d while sim was ongoing. Check the code.\n", terminated_p, WEXITSTATUS(status));
            cleanup(users, nodes, shm_id, sem_id);
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
        cleanup(users, nodes, shm_id, sem_id);
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

    ending_reason = SIM_END_SEC;
    if (SO_USERS_NUM == 0) {
      ending_reason = SIM_END_USR;
    }
    else if (book->cursor == SO_REGISTRY_SIZE) {
      ending_reason = SIM_END_SIZ;
    }
    summary_print(ending_reason, nof_transactions, nodes);
    free(nof_transactions);
  }

  free_list(users);
  free_list(nodes);

  if (semctl(sem_id, 0, IPC_RMID) == -1) {
    fprintf(ERR_FILE, "master: could not free sem %d, please check ipcs and remove it.\n", sem_id);
    exit(EXIT_FAILURE);
  }
  fprintf(LOG_FILE, "Completed simulation. Exiting...\n");
  exit(EXIT_SUCCESS);
}

