#include "../utils/utils.h"
#include "../load_constants/load_constants.h"
#include "../pid_list/pid_list.h"
#include "../master_book/master_book.h"
#include "master_functions/master_functions.h"
#include "../node/node.h"
#include "../user/user.h"
#include "master.h"

#include <sys/time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
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

#define TEST_ERROR_AND_FAIL    if (errno && errno != ESRCH) {fprintf(ERR_FILE, \
                       "%s:%d: PID=%5d: Error %d (%s)\n",\
                       __FILE__,\
                       __LINE__,\
                       getpid(),\
                       errno,\
                       strerror(errno)); \
                       master_cleanup(); \
                       exit(EXIT_FAILURE); }

#define SIM_END_SEC 0
#define SIM_END_USR 1
#define SIM_END_SIZ 2

#define MAX_USERS_TO_PRINT 10
#define SO_REGISTRY_SIZE 50

#define ID_READY_ALL 0
#define ID_MEM 1
#define ID_READY_NODE 2
#define NUM_SEM 3

#define REGISTRY_SIZE sizeof(transaction) * SO_BLOCK_SIZE * SO_REGISTRY_SIZE

extern int SO_USERS_NUM;
extern int SO_NODES_NUM;
extern int SO_SIM_SEC;
extern int SO_NUM_FRIENDS;
extern int SO_BUDGET_INIT;

static int* users = NULL;

struct nodes nodes = { NULL, NULL };
struct master_book book;

static int* nodes_write_fd;

static int queue_id = -1;
static int sem_id = -1;
static int shm_book_id = -1;
static int shm_book_size_id = -1;
static int shm_nodes_array_id = -1;
static int shm_nodes_size_id = -1;
static int shm_users_array_id = -1;


static int* user_budget;
static int* node_budget;
static int block_reached;

static int inactive_users;
static int simulation_seconds = 0;
static int stop_sim = 0;

void periodical_update() {
  int index;
  transaction t;
  int i = block_reached;
  int size = *book.size;

  while (i < size) {
    t = book.blocks[i * SO_BLOCK_SIZE];
    if ((index = find_element(users, SO_USERS_NUM, t.sender)) != -1) {
      user_budget[index] -= (t.quantita + t.reward);
      index = find_element(users, SO_USERS_NUM, t.receiver);
      user_budget[index] += t.quantita;
    }
    i++;
  }
  index = find_element(nodes.array, *nodes.size, t.receiver);
  node_budget[index] += t.quantita;
  block_reached = i / SO_BLOCK_SIZE;
}

void stop_simulation() {
  int i = 0;
  int child = 0;
  while ((child = users[i]) != 0 && i < SO_USERS_NUM) {
    i++;
    kill(child, SIGTERM);
  }
  i = 0;
  errno = 0;
  while ((child = nodes.array[i]) != 0 && i < *nodes.size) {
    i++;
    kill(child, SIGTERM);
  }
}

void master_cleanup() {
  if (users != NULL && nodes.array != NULL) {
    stop_simulation();
  }

  if (shm_book_id != -1) {
    shmctl(shm_book_id, IPC_RMID, 0);
    TEST_ERROR;
  }

  if (shm_book_id != -1) {
    shmctl(shm_book_id, IPC_RMID, 0);
    TEST_ERROR;
  }

  if (shm_nodes_array_id != -1) {
    shmctl(shm_nodes_array_id, IPC_RMID, 0);
    TEST_ERROR;
  }

  if (shm_nodes_size_id != -1) {
    shmctl(shm_nodes_size_id, IPC_RMID, 0);
    TEST_ERROR;
  }

  if (sem_id != -1) {
    semctl(sem_id, IPC_RMID, 0);
    TEST_ERROR;
  }

  if (queue_id != -1) {
    msgctl(queue_id, IPC_RMID, 0);
    TEST_ERROR;
  }

}

int  start_shared_memory(int key, size_t size) {
  return shmget(key, size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
}

void periodical_print() {
  int i = 0;
  fprintf(LOG_FILE, "NUMBER OF ACTIVE USERS %d | NUMBER OF ACTIVE NODES %d\n", SO_USERS_NUM - inactive_users, *nodes.size);
  /* budget di ogni processo utente (inclusi quelli terminati prematuramente)*/
  fprintf(LOG_FILE, "USERS BUDGETS\n");
  if (SO_USERS_NUM < MAX_USERS_TO_PRINT) {
    while (i < SO_USERS_NUM) {
      fprintf(LOG_FILE, "USER u%d : %d$\n", users[i], user_budget[i++]);
    }
  }
  else {
    int min_i = 0;
    int max_i = 0;
    fprintf(LOG_FILE, "[!] Users count is too high to display all budgets [!]\n");
    i = 1;
    while (i < SO_NODES_NUM) {
      if (user_budget[i] < user_budget[min_i]) {
        min_i = i;
      }
      if (user_budget[i] > user_budget[max_i]) {
        max_i = i;
      }
      i++;
    }

    fprintf(LOG_FILE, "HIGHEST BUDGET: USER u%d : %d$\n", users[max_i], user_budget[max_i]);
    fprintf(LOG_FILE, "LOWEST BUDGET: USER u%d : %d$\n", users[min_i], user_budget[min_i]);
  }
  /* budget di ogni processo nodo */
  i = 0;
  fprintf(LOG_FILE, "NODES BUDGETS\n");
  while (i < SO_NODES_NUM) {
    fprintf(LOG_FILE, "NODE n%d : %d$\n", nodes.array[i], node_budget[i++]);
  }
}

void summary_print(int ending_reason, int* users, int* user_budget, int* nodes, int* node_budget, int* nof_transactions) {
  int i = 0;

  switch (ending_reason)
  {
  case SIM_END_SEC:
    fprintf(LOG_FILE, "[!] Simulation ending reason: TIME LIMIT REACHED [!]\n\n");
    break;
  case SIM_END_SIZ:
    fprintf(LOG_FILE, "[!] Simulation ending reason: MASTER BOOK SIZE EXCEEDED [!]\n\n");
    break;
  case SIM_END_USR:
    fprintf(LOG_FILE, "[!] Simulation ending reason: ALL USERS TERMINATED [!]\n\n");
  default:
    fprintf(LOG_FILE, "[!] Simulation ending reason: UNEXPECTED ERRORS [!]\n\n");
    break;
  }
  /* bilancio di ogni processo utente, compresi quelli che sono terminati prematuramente */
  fprintf(LOG_FILE, "USERS BUDGETS\n");
  while (i < SO_USERS_NUM) {
    fprintf(LOG_FILE, "USER u%d : %d$\n", users[i], user_budget[i++]);
  }
  /* bilancio di ogni processo nodo */
  i = 0;
  fprintf(LOG_FILE, "NODES BUDGETS\n");
  while (i < SO_NODES_NUM) {
    fprintf(LOG_FILE, "NODE n%d : %d$\n", nodes[i], node_budget[i++]);
  }
  fprintf(LOG_FILE, "NUMBER OF INACTIVE USERS: %d\n", inactive_users);
  fprintf(LOG_FILE, "NUMBER OF TRANSACTION BLOCK WRITTEN INTO THE MASTER BOOK: %d\n", *book.size);

  fprintf(LOG_FILE, "\n\n");

  fprintf(LOG_FILE, "Number of transactions left per node:\n");
  while (i < SO_NODES_NUM) {
    fprintf(LOG_FILE, "NODE %d: %d transactions left\n", nodes[i], nof_transactions[i]);
    i++;
  }
}

void wait_siblings(int sem_num) {
  struct sembuf sops;

  sops.sem_num = sem_num;
  sops.sem_op = -1;
  semop(sem_id, &sops, 1);

  fprintf(LOG_FILE, "CHILD %d: waiting for green light.\n", getpid());
  sops.sem_op = 0;
  semop(sem_id, &sops, 1);
}

int* assign_friends(int* array, int size) {
  int* friends = init_list(SO_NUM_FRIENDS);
  int i = 0;
  if (friends == NULL)
    return NULL;

  while (i < SO_NUM_FRIENDS) {
    int random_el = random_element(array, size);
    if (random_el == -1) {
      fprintf(ERR_FILE, "%s:%d: something went wrong with the extraction of friends\n", __FILE__, __LINE__);
      free_list(friends);
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
    simulation_seconds++;
    periodical_update();
    periodical_print();
    alarm(1);
    break;
  case SIGINT:
    fprintf(LOG_FILE, "master: recieved a SIGINT at %ds from simulation start\n", simulation_seconds);
    master_cleanup();
    exit(EXIT_FAILURE);
    break;
  case SIGUSR1:
  {
    int child;
    transaction arriva;
    int file_descriptors[2];
    struct sembuf sops;
    int i = 0;
    int new_node_msg_id;

    pipe(file_descriptors);
    TEST_ERROR_AND_FAIL;

    msgrcv(queue_id, &arriva, sizeof(transaction), 0, IPC_NOWAIT);
    TEST_ERROR_AND_FAIL;

    if (*nodes.size < SO_NODES_NUM * 2) {
      switch ((child = fork())) {
      case -1:
        fprintf(ERR_FILE, "%s:%d: fork failed for node creation.\n", __FILE__, __LINE__);
        master_cleanup();
        exit(EXIT_FAILURE);
        break;
      case 0:
      {
        int* friends;
        struct nodes nodes;
        nodes.array = attach_shm_memory(shm_nodes_array_id);
        TEST_ERROR_AND_FAIL;
        nodes.size = attach_shm_memory(shm_nodes_size_id);
        TEST_ERROR_AND_FAIL;

        close(file_descriptors[1]);
        friends = assign_friends(nodes.array, *nodes.size);
        if (friends == NULL) {
          kill(getppid(), SIGTERM);
          exit(EXIT_FAILURE);
        }

        init_node(friends, file_descriptors[0], shm_book_id, shm_book_size_id);
        break;
      }
      default:
      {
        int* lista_nodi = init_list(SO_NUM_FRIENDS);
        TEST_ERROR_AND_FAIL;
        i = 0;
        close(file_descriptors[0]);
        while (i < SO_NUM_FRIENDS) {
          int node_random = random_element(nodes.array, *nodes.size);
          if (node_random == -1) {
            fprintf(ERR_FILE, "%s:%d: something went wrong with the extraction of the node\n", __FILE__, __LINE__);
            master_cleanup();
            exit(EXIT_FAILURE);
          }
          if (find_element(lista_nodi, SO_NUM_FRIENDS, node_random) == -1) {
            lista_nodi[i++] = node_random;
            write(file_descriptors[1], &child, sizeof(int));
            kill(node_random, SIGUSR2);
          }
        }
        nodes_write_fd[*nodes.size] = file_descriptors[1];
        break;
      }
      }

      sops.sem_num = ID_READY_ALL;
      sops.sem_op = 1;
      semop(sem_id, &sops, 1);

      sops.sem_op = 0;
      semop(sem_id, &sops, 1);

      /*msgget del nodo */
      if ((new_node_msg_id = msgget(child, S_IWUSR)) == -1) {
        fprintf(ERR_FILE, "master: cannot retrieve the node queue, node=%d, error %s\n", child, strerror(errno));
        master_cleanup();
        exit(EXIT_FAILURE);

      }
      /*msgsnd della transaction al nodo */
      if (msgsnd(new_node_msg_id, &arriva, sizeof(transaction), IPC_NOWAIT) == -1) {
        if (errno != EAGAIN) {
          fprintf(ERR_FILE, "master n%d: recieved an unexpected error while sending transaction: %s.\n", getpid(), strerror(errno));
        }

      }

      kill(child, SIGUSR1);

      i = 0;
      nodes.array[*nodes.size++] = child;
    }
    else {
      fprintf(ERR_FILE, "master: transaction refused\n");
      kill(arriva.sender, SIGUSR2);
    }
    break;
  }
  }
}

int main() {
  struct sembuf sops;
  sigset_t mask;
  struct sigaction act;
  struct msqid_ds stats;
  int* nodes_array;
  transaction** registry;
  int* size_nodes;
  int* size_book;
  int i = 0;
  bzero(&stats, sizeof(struct msqid_ds));
  bzero(&sops, sizeof(struct sembuf));
  load_constants();

  if ((nodes_write_fd = init_list(SO_NODES_NUM * 2)) == NULL) {
    fprintf(ERR_FILE, "master: cannot allocate memory for nodes_write_fd.\n");
    exit(EXIT_FAILURE);
  }
  if ((node_budget = init_list(SO_NODES_NUM * 2)) == NULL) {
    fprintf(ERR_FILE, "master: cannot allocate memory for node_budget.\n");
    exit(EXIT_FAILURE);
  }
  if ((user_budget = init_list(SO_USERS_NUM)) == NULL) {
    fprintf(ERR_FILE, "master: cannot allocate memory for user_budget.\n");
    exit(EXIT_FAILURE);
  }

  while (i < SO_USERS_NUM) {
    user_budget[i++] = SO_BUDGET_INIT;
  }

  fprintf(LOG_FILE, "[!] MASTER PID: %d\n", getpid());

  bzero(&mask, sizeof(sigset_t));
  sigemptyset(&mask);
  act.sa_handler = handler;
  act.sa_flags = 0;
  act.sa_mask = mask;

  /* Inizializzazione semafori start sincronizzato e write del libro mastro */
  sem_id = semget(getpid(), NUM_SEM, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
  TEST_ERROR_AND_FAIL;
  semctl(sem_id, ID_MEM, SETVAL, 1);
  TEST_ERROR_AND_FAIL;
  semctl(sem_id, ID_READY_ALL, SETVAL, SO_USERS_NUM + SO_NODES_NUM + 1);
  TEST_ERROR_AND_FAIL;
  semctl(sem_id, ID_READY_NODE, SETVAL, SO_NODES_NUM + 1);
  TEST_ERROR_AND_FAIL;

  /* Inizializzazione libro mastro */
  shm_book_id = start_shared_memory(IPC_PRIVATE, sizeof(transaction) * SO_BLOCK_SIZE * SO_REGISTRY_SIZE);
  TEST_ERROR_AND_FAIL;
  book.blocks = attach_shm_memory(shm_book_id);
  TEST_ERROR_AND_FAIL;
  shm_book_size_id = start_shared_memory(IPC_PRIVATE, sizeof(int));
  TEST_ERROR_AND_FAIL;
  book.size = attach_shm_memory(shm_book_size_id);
  TEST_ERROR_AND_FAIL;

  /* Inizializzazione shm nodi array */
  shm_nodes_array_id = start_shared_memory(IPC_PRIVATE, sizeof(int) * SO_NODES_NUM * 2);
  TEST_ERROR_AND_FAIL;
  nodes.array = attach_shm_memory(shm_nodes_array_id);
  TEST_ERROR_AND_FAIL;

  /* Inizializzazione shm per size dei nodi */
  shm_nodes_size_id = start_shared_memory(IPC_PRIVATE, sizeof(int));
  TEST_ERROR_AND_FAIL;
  nodes.size = attach_shm_memory(shm_nodes_size_id);
  TEST_ERROR_AND_FAIL;

  *nodes.size = SO_NODES_NUM;

  /* Inizializzazione shm users array */
  shm_users_array_id = start_shared_memory(IPC_PRIVATE, sizeof(int) * SO_USERS_NUM);
  TEST_ERROR_AND_FAIL;
  users = attach_shm_memory(shm_users_array_id);
  TEST_ERROR_AND_FAIL;

  /* Inizializzazione coda di messaggi master-nodi */
  queue_id = msgget(getpid(), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
  TEST_ERROR_AND_FAIL;
  fprintf(LOG_FILE, "master: inizializzata la coda con key %d e id %d\n", getpid(), queue_id);
  msgctl(queue_id, IPC_STAT, &stats);
  TEST_ERROR_AND_FAIL;
  stats.msg_qbytes = sizeof(transaction) * (long unsigned int) SO_NODES_NUM;
  msgctl(queue_id, IPC_SET, &stats);
  TEST_ERROR_AND_FAIL;

  /* Handling segnali SIGALRM, SIGINT, SIGUSR1 */
  if (sigaction(SIGALRM, &act, NULL) < 0) {
    fprintf(ERR_FILE, "master: could not associate handler to SIGALRM.\n");
    master_cleanup();
    exit(EXIT_FAILURE);
  }
  if (sigaction(SIGINT, &act, NULL) < 0) {
    fprintf(ERR_FILE, "master: could not associate handler to SIGINT.\n");
    master_cleanup();
    exit(EXIT_FAILURE);
  }
  if (sigaction(SIGUSR1, &act, NULL) < 0) {
    fprintf(ERR_FILE, "master: could not associate handler to SIGUSR1.\n");
    master_cleanup();
    exit(EXIT_FAILURE);
  }
  /* Fine handling segnali */

  i = 0;
  while (i < SO_NODES_NUM) {
    int fd[2];
    int child;
    if (pipe(fd) == -1) {
      fprintf(ERR_FILE, "master: error while creating pipe in iteration %d/%d", i + 1, SO_NODES_NUM);
      master_cleanup();
    }
    switch ((child = fork())) {
    case -1:
      fprintf(ERR_FILE, "master: fork failed for node creation iteration %d/%d.\n", i + 1, SO_NODES_NUM);
      master_cleanup();
      exit(EXIT_FAILURE);
    case 0:
    {
      int* friends;
      int* nodes_arr;
      if ((nodes_arr = attach_shm_memory(shm_nodes_array_id)) == NULL) {
        fprintf(ERR_FILE, "u%d: cannot attach nodes shared memory\n", getpid());
        exit(EXIT_FAILURE);
      }
      close(fd[1]);

      wait_siblings(ID_READY_NODE);
      if ((friends = assign_friends(nodes_arr, SO_NODES_NUM)) == NULL) {
        fprintf(ERR_FILE, "master: cannot assign friends to node %d\n", getpid());
        exit(EXIT_FAILURE);
      }
      init_node(friends, fd[0], shm_book_id, shm_book_size_id);
      break;
    }
    default:
      close(fd[0]);
      nodes_write_fd[i] = fd[1];
      nodes.array[i] = child;
      i++;
      break;
    }
  }

  /* Do la possibilità ai nodi di completare il setup */
  sops.sem_num = ID_READY_NODE;
  sops.sem_op = -1;
  semop(sem_id, &sops, 1);

  sops.sem_op = 0;
  semop(sem_id, &sops, 1);

  i = 0;
  while (i < SO_USERS_NUM) {
    int child;

    switch ((child = fork())) {
    case -1:
      fprintf(ERR_FILE, "master: fork failed for user creation iteration %d/%d.\n", i + 1, SO_USERS_NUM);
      master_cleanup();
      exit(EXIT_FAILURE);
    case 0:
    {
      int* users;
      if ((users = attach_shm_memory(shm_users_array_id)) == NULL) {
        fprintf(ERR_FILE, "u%d: cannot attach users shared memory\n", getpid());
        exit(EXIT_FAILURE);
      }
      wait_siblings(ID_READY_ALL);
      init_user(users, shm_nodes_array_id, shm_nodes_size_id, shm_book_id, shm_book_size_id);
      break;
    }
    default:
      users[i++] = child;
      break;
    }
  }

  sops.sem_num = ID_READY_ALL;
  sops.sem_op = -1;
  semop(sem_id, &sops, 1);

  sops.sem_num = ID_READY_ALL;
  sops.sem_op = 0;
  semop(sem_id, &sops, 1);

  fprintf(LOG_FILE, "master: inizio il ciclo principale\n");
  {
    int* nof_transactions = malloc(sizeof(int) * SO_NODES_NUM * 2);
    int ending_reason;

    int i = 0;
    alarm(1);
    while (!stop_sim && simulation_seconds < SO_SIM_SEC && inactive_users < SO_USERS_NUM && *book.size < SO_REGISTRY_SIZE) {
      int status = 0;
      int node_index = 0;
      int terminated_p = wait(&status);
      if (terminated_p == -1) {
        if (errno == EINTR) {
          continue;
        }
        master_cleanup();
        fprintf(ERR_FILE, "master: wait failed for an unexpected error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
      }
      if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) == EXIT_FAILURE) {
          fprintf(ERR_FILE, "Child %d encountered an error. Stopping simulation\n", terminated_p);
          master_cleanup();
          free(nof_transactions);
          exit(EXIT_FAILURE);
        }
        else {
          if ((node_index = find_element(nodes.array, *nodes.size, terminated_p)) != -1) {
            fprintf(ERR_FILE, "master: node %d terminated with the unexpected status %d while sim was ongoing. Check the code.\n", terminated_p, WEXITSTATUS(status));
            master_cleanup();
            exit(EXIT_FAILURE);
          }
          else {
            inactive_users++;
          }
        }
      }

    }
    alarm(0);
    stop_simulation();

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
        master_cleanup();
        exit(EXIT_FAILURE);
      }
      if ((index = find_element(nodes.array, SO_NODES_NUM, terminated_p)) != -1) {
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

    ending_reason = -1;
    if (simulation_seconds == SO_SIM_SEC) {
      ending_reason = SIM_END_SEC;
    }
    else if (SO_USERS_NUM == 0) {
      ending_reason = SIM_END_USR;
    }
    else if (*book.size == SO_REGISTRY_SIZE) {
      ending_reason = SIM_END_SIZ;
    }

    periodical_update();
    summary_print(ending_reason, users, user_budget, nodes.array, node_budget, nof_transactions);
    free(nof_transactions);
  }

  if (shmctl(shm_book_id, IPC_RMID, NULL) == -1) {
    fprintf(ERR_FILE, "master: error in shmctl while removing the shm with id %d. Please check ipcs and remove it.\n", shm_book_id);
    exit(EXIT_FAILURE);
  }
  if (shmctl(shm_nodes_array_id, IPC_RMID, NULL) == -1) {
    fprintf(ERR_FILE, "master: error in shmctl while removing the shm with id %d. Please check ipcs and remove it.\n", shm_nodes_array_id);
    exit(EXIT_FAILURE);
  }

  if (semctl(sem_id, 0, IPC_RMID) == -1) {
    fprintf(ERR_FILE, "master: could not free sem %d, please check ipcs and remove it.\n", sem_id);
    exit(EXIT_FAILURE);
  }
  fprintf(LOG_FILE, "Completed simulation. Exiting...\n");
  exit(EXIT_SUCCESS);
}

