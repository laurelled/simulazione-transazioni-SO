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

#define EARLY_FAILURE 69

#define MAX_USERS_TO_PRINT 10
#define SO_REGISTRY_SIZE 10

#define ID_READY 0
#define NUM_SEM 2

#define SHM_SIZE sizeof(transaction) * SO_BLOCK_SIZE * SO_REGISTRY_SIZE

struct master_book {/*da cambiare nome*/
  unsigned int cursor;
  transaction** blocks;
};

struct master_book* book;

extern int SO_USERS_NUM;
extern int SO_NODES_NUM;
extern int SO_SIM_SEC;

static int inactive_users;

static int shm_id;

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

void summary_print() {
  /* TODO: bilancio di ogni processo utente, compresi quelli che sono terminati prematuramente */
  /* TODO: bilancio di ogni processo nodo */
  fprintf(LOG_FILE, "NUMBER OF INACTIVE USERS: %d\n", inactive_users);
  fprintf(LOG_FILE, "NUMBER OF TRANSACTION BLOCK WRITTEN INTO THE MASTER BOOK: %d\n", book->cursor);
  /* TODO: per ogni processo nodo, numero di transazioni ancora presenti nella transaction pool. Forse farlo ritornare come status? */


}

int main() {
  pid_t* users = init_list(SO_USERS_NUM);
  pid_t* nodes = init_list(SO_NODES_NUM);

  int i = 0;
  struct sembuf sops;
  int sem_id;

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

  i = 0;
  while (i < SO_SIM_SEC && inactive_users < SO_USERS_NUM && book->cursor < SO_REGISTRY_SIZE) {
    int status = 0;
    int terminated_p = wait(&status);
    if (terminated_p == -1) {
      if (errno == EINTR) {
        continue;
      }
      fprintf(ERR_FILE, "master, che cazzo ci fai qui\n");
      exit(EXIT_FAILURE);
    }
    if (list_contains(nodes, SO_NODES_NUM, terminated_p)) {
      if (WIFEXITED(status)) {
        switch (WEXITSTATUS(status)) {
        case EXIT_SUCCESS:
          break;
        case EXIT_FAILURE:
          break;
        case EARLY_FAILURE:
          break;
        default:
          break;
        }
      }
    }
    else {
      inactive_users++;
    }

  }

  stop_simulation(users, nodes);
  fprintf(LOG_FILE, "End simulation reason: ");
  if (i == SO_SIM_SEC) {
    fprintf(LOG_FILE, "SIMULATION TIME RUN OFF\n");
  }
  else if (SO_USERS_NUM == 0) {
    fprintf(LOG_FILE, "ACTIVE USERS REACHED 0\n");
  }
  else if (book->cursor == SO_REGISTRY_SIZE) {
    fprintf(LOG_FILE, "MASTER BOOK CAPACITY REACHED MAXIMUM CAPACITY\n");
  }
  summary_print();
  free_list(users);
  free_list(nodes);
  exit(EXIT_SUCCESS);
}
