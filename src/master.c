#include "utils/utils.h"
#include "load_constants/load_constants.h"
#include "transaction.h"
#include "node/node.h"
#include "user/user.h"

#include <sys/time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <signal.h>
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

static int* users;
static int* nodes;

static int inactive_users;

static int shm_id;

void stop_simulation() {
  int i = 0;
  while (i < SO_USERS_NUM) {
    kill(users[i++], SIGTERM);
  }
  i = 0;
  while (i < SO_NODES_NUM) {
    kill(nodes[i++], SIGTERM);
  }

  free(users);
  free(nodes);
}

void cleanup() {
  stop_simulation();

  if (shmctl(shm_id, 0, IPC_RMID) == -1) {
    fprintf(ERR_FILE, "%s cleanup: cannot remove shm with id %d\n", __FILE__, shm_id);
  }
}

int  start_shared_memory() {
  int shid;
  if (shid = shmget(getpid(), SHM_SIZE, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR) == -1) {
    fprintf(ERR_FILE, "master:error on shm creation\n");
    exit(EXIT_FAILURE);
  }

  return shid;

}

struct master_book* get_master_book(int shm_id) {
  struct master_book* new = malloc(sizeof(struct master_book));
  transaction** ptr;
  if ((ptr = shmat(shm_id, NULL, 0)) == -1) {
    fprintf(ERR_FILE, "master: the process can't be attached\n");
    cleanup();
    exit(EXIT_FAILURE);
  }
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

void init_master() {
  users = (int*)calloc(SO_USERS_NUM, sizeof(pid_t));
  nodes = (int*)calloc(SO_NODES_NUM, sizeof(pid_t));
}

void summary_print() {
  //TODO: bilancio di ogni processo utente, compresi quelli che sono terminati prematuramente
  // TODO: bilancio di ogni processo nodo
  fprintf(LOG_FILE, "NUMBER OF INACTIVE USERS: %d\n", inactive_users);
  fprintf(LOG_FILE, "NUMBER OF TRANSACTION BLOCK WRITTEN INTO THE MASTER BOOK: %d\n", book->cursor);
  //TODO: per ogni processo nodo, numero di transazioni ancora presenti nella transaction pool. Forse farlo ritornare come status?


}

int main() {
  int i = 0;
  struct sembuf sops;
  int sem_id;
  shm_id = start_shared_memory();
  book = get_master_book(shm_id);

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
      cleanup();
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
      cleanup();
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
    int ret_val = wait(&status);
    if (ret_val == -1) {
      if (errno == EINTR) {
        continue;
      }
      fprintf(ERR_FILE, "master, che cazzo ci fai qui\n");
      exit(EXIT_FAILURE);
    }
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
      inactive_users++;
    }

  }

  stop_simulation();
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

  exit(EXIT_SUCCESS);
}
