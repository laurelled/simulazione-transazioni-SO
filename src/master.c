#include "utils/utils.h"
#include "load_constants/load_constants.h"
#include "pid_list/pid_list.h"
#include "master_book/master_book.h"
#include "node/node.h"
#include "user/user.h"

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

#define SIM_END_SEC 0
#define SIM_END_USR 1
#define SIM_END_SIZ 2

#define NUM_PROC SO_NODES_NUM + SO_USERS_NUM + 1

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
static int* nodes_write_fd;
static int nof_nodes;

static int queue_id;
static int sem_id = -1;


static int* user_budget;
static int* node_budget;
static int block_reached;

static int inactive_users;
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

void cleanup(pid_t* users, pid_t* nodes, int shm_id, int sem_id, int queue_id) {
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

  if (queue_id != -1) {
    if (msgctl(queue_id, 0, IPC_RMID) == -1) {
      fprintf(ERR_FILE, "master: cannot remove msg queue with id %d\n. Please check ipcs and remove it.\n", queue_id);
    }
  }
}

int  start_shared_memory(int key, size_t size) {
  return shmget(key, size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
}

void periodical_print(pid_t* users, int* user_budget, int* node_budget) {
  int i = 0;
  fprintf(LOG_FILE, "NUMBER OF ACTIVE USERS %d | NUMBER OF ACTIVE NODES %d", SO_USERS_NUM - inactive_users, SO_NODES_NUM);
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
    fprintf(LOG_FILE, "[!] Users count is too high to display all budgets [!]");
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

    fprintf(LOG_FILE, "HIGHEST BUDGET: USER u%d : %d$", users[max_i], user_budget[max_i]);
    fprintf(LOG_FILE, "LOWEST BUDGET: USER u%d : %d$", users[min_i], user_budget[min_i]);
  }
  /* budget di ogni processo nodo */
  i = 0;
  fprintf(LOG_FILE, "NODES BUDGETS\n");
  while (i < SO_NODES_NUM) {
    fprintf(LOG_FILE, "NODE n%d : %d$\n", nodes[i], node_budget[i++]);
  }
}

void summary_print(int ending_reason, pid_t* users, int* user_budget, pid_t* nodes, pid_t* node_budget, int* nof_transactions) {
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
    simulation_seconds++;
    periodical_update(users, user_budget, node_budget);
    periodical_print(users, user_budget, node_budget);
    alarm(1);
    break;
  case SIGINT:
    stop_sim = 1;
    fprintf(LOG_FILE, "master: recieved a SIGTERM at %ds from simulation start\n", simulation_seconds);
    break;
  case SIGUSR1:
  {
    transaction arriva;
    int file_descriptors[2];
    struct sembuf sops;

    pipe(file_descriptors);
    pid_t child;
    msgrcv(queue_id, &arriva, sizeof(transaction), 0, IPC_NOWAIT);
    switch ((child = fork())) {
    case -1:
      fprintf(ERR_FILE, "master handler: fork failed for node creation.\n");
      /*TODO: cleanup(users, nodes, shm_id, sem_id);  parametri da vedere*/
      exit(EXIT_FAILURE);
      break;
    case 0:
    {
      pid_t* friends;
      close(file_descriptors[1]);
      friends = assign_friends(nodes);

      init_node(friends, file_descriptors[0]);
      break;
    }
    default:
    {
      int i = 0;
      pid_t* lista_nodi = init_list(SO_NUM_FRIENDS);
      close(file_descriptors[0]);
      while (i < SO_NUM_FRIENDS) {
        pid_t node_random = random_element(nodes, nof_nodes);
        if (find_element(lista_nodi, SO_NUM_FRIENDS, node_random) == -1) {
          lista_nodi[i++] = node_random;
          write(file_descriptors[1], &child, sizeof(pid_t));
          kill(node_random, SIGUSR2);
        }
      }

      nodes_write_fd[nof_nodes] = file_descriptors[1];
      break;
    }
    }
    int new_node_msg_id;
    int i = 0;

    sops.sem_num = sem_id;
    sops.sem_op = 1;
    semop(sem_id, &sops, 1);

    sops.sem_op = 0;
    semop(sem_id, &sops, 1);



    /*msgget del nodo */
    if ((new_node_msg_id = msgget(child, 0)) == -1) {
      fprintf(ERR_FILE, "master: cannot retrieve the node queue, node= %d\n", child);
      /*TODO: cleanup(users, nodes, shm_id, sem_id, queue_id); classico dubbio dei parametri*/
      exit(EXIT_FAILURE);

    }
    /*msgsnd della transaction al nodo */
    if (msgsnd(new_node_msg_id, &arriva, sizeof(transaction), IPC_NOWAIT) == -1) {
      if (errno != EAGAIN) {
        fprintf(ERR_FILE, "master n%d: recieved an unexpected error while sending transaction: %s.\n", getpid(), strerror(errno));
      }

    }
    kill(child, SIGUSR1);

    /* TODO: sostituire nodes con una shm*/
    expandlist(nodes, nof_nodes, nof_nodes + 1);
    nodes[nof_nodes++] = child;
    while (i < SO_USERS_NUM) {
      kill(users[i], SIGUSR2);
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
  struct sembuf sops;
  int shm_id = -1;
  int shm_nodes = -1;
  users = init_list(SO_USERS_NUM);
  nodes = init_list(SO_NODES_NUM);
  nof_nodes = SO_NODES_NUM;


  fprintf(LOG_FILE, "[!] MASTER PID: %d\n", getpid());

  bzero(&mask, sizeof(sigset_t));
  sigemptyset(&mask);
  act.sa_handler = handler;
  act.sa_flags = 0;
  act.sa_mask = mask;

  /* Handling segnali SIGALRM, SIGINT, SIGUSR1 */
  if (sigaction(SIGALRM, &act, NULL) < 0) {
    fprintf(ERR_FILE, "master: could not associate handler to SIGALRM.\n");
    cleanup(users, nodes, shm_id, sem_id, queue_id);
    exit(EXIT_FAILURE);
  }
  if (sigaction(SIGINT, &act, NULL) < 0) {
    fprintf(ERR_FILE, "master: could not associate handler to SIGTERM.\n");
    cleanup(users, nodes, shm_id, sem_id, queue_id);
    exit(EXIT_FAILURE);
  }
  if (sigaction(SIGUSR1, &act, NULL) < 0) {
    fprintf(ERR_FILE, "master: could not associate handler to SIGTERM.\n");
    cleanup(users, nodes, shm_id, sem_id, queue_id);
    exit(EXIT_FAILURE);
  }
  /* Fine handling segnali */

  /* Inizializzazione semafori start sincronizzato e write del libro mastro */
  if ((sem_id = semget(getpid(), NUM_SEM, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR)) == -1) {
    fprintf(ERR_FILE, "master: semaphore %d already exists.\n", getpid());
    free_list(users);
    free_list(nodes);
    exit(EXIT_FAILURE);
  }

  if (semctl(sem_id, ID_READY, SETVAL, NUM_PROC) == -1) {
    fprintf(ERR_FILE, "master: cannot set initial value for sem ID_READY with id %d.\n", sem_id);
    cleanup(users, nodes, shm_id, sem_id, queue_id);
    exit(EXIT_FAILURE);
  }

  if (semctl(sem_id, ID_MEM, SETVAL, 1) == -1) {
    fprintf(ERR_FILE, "master: cannot set initial value for sem ID_MEM with id %d.\n", sem_id);
    cleanup(users, nodes, shm_id, sem_id, queue_id);
    exit(EXIT_FAILURE);
  }
  /* Fine inizializzazione semafori */

  /* Inizializzazione libro mastro */
  if ((shm_id = start_shared_memory(getpid(), sizeof(struct master_book))) == -1) {
    fprintf(ERR_FILE, "master: cannot start shared memory. Please clear the shm with id %d\n", getpid());
    cleanup(users, nodes, shm_id, sem_id, queue_id);
    exit(EXIT_FAILURE);
  }
  /* Inizializzazione shm nodi */
  if ((shm_nodes = start_shared_memory(1, sizeof(pid_t*))) == -1) {
    fprintf(ERR_FILE, "master: cannot start shared memory. Please clear the shm with id %d\n", getpid());
    cleanup(users, nodes, shm_id, sem_id, queue_id);
    exit(EXIT_FAILURE);
  }

  if ((book = get_master_book(shm_id)) == NULL) {
    fprintf(ERR_FILE, "master: the process cannot be attached to the shared memory.\n");
    cleanup(users, nodes, shm_id, sem_id, queue_id);
    exit(EXIT_FAILURE);
  }
  if ((book->blocks = malloc(REGISTRY_SIZE)) == NULL) {
    fprintf(ERR_FILE, "master: cannot allocate memory for master book transactions blocks. Check memory usage\n");
    cleanup(users, nodes, shm_id, sem_id, queue_id);
    exit(EXIT_FAILURE);
  }
  book->cursor = 0;
  /*Fine gestione libro mastro*/

  /* Inizializzazione coda di messaggi master-nodi */
  if (queue_id = msgget(getpid(), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR) == -1) {
    fprintf(ERR_FILE, "master: cannot create msg queue. pls delete the queue with id %d\n ", getpid());
    cleanup(users, nodes, shm_id, sem_id, queue_id);
    exit(EXIT_FAILURE);
  }
  if (msgctl(queue_id, IPC_STAT, &stats) < 0) {
    fprintf(ERR_FILE, "init_node: cannot read msgqueue stats. Check user permission.\n");
    cleanup(users, nodes, shm_id, sem_id, queue_id);
    exit(EXIT_FAILURE);
  }
  stats.msg_qbytes = sizeof(transaction) * (long unsigned int) SO_NODES_NUM;
  if (msgctl(queue_id, IPC_SET, &stats) < 0) {
    fprintf(ERR_FILE, "init_node: cannot write msgqueue stats. Check user permission.\n");
    cleanup(users, nodes, shm_id, sem_id, queue_id);
    exit(EXIT_FAILURE);
  }
  /*  Fine inizializzazione coda di messaggi master-nodi  */

  {
    int i = 0;
    while (i < SO_USERS_NUM) {
      pid_t child;

      switch ((child = fork())) {
      case -1:
        fprintf(ERR_FILE, "master: fork failed for user creation iteration %d/%d.\n", i + 1, SO_USERS_NUM);
        cleanup(users, nodes, shm_id, sem_id, queue_id);
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
      int fd[2];
      pid_t child;
      if (pipe(fd) == -1) {
        fprintf(ERR_FILE, "master: error while creating pipe in iteration %d/%d", i + 1, SO_NODES_NUM);
        cleanup(users, nodes, shm_id, sem_id, queue_id);
      }
      switch ((child = fork())) {
      case -1:
        fprintf(ERR_FILE, "master: fork failed for node creation iteration %d/%d.\n", i + 1, SO_NODES_NUM);
        cleanup(users, nodes, shm_id, sem_id, queue_id);
        exit(EXIT_FAILURE);
      case 0:
      {
        pid_t* friends;
        close(fd[1]);
        /* wait_siblings(sem_id, ID_READY); */
        friends = assign_friends(nodes);
        init_node(friends, fd[0]);
        break;
      }
      default:
        close(fd[0]);
        nodes_write_fd[i] = fd[1];
        nodes[i] = child;
        i++;
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
        cleanup(users, nodes, shm_id, sem_id, queue_id);
        fprintf(ERR_FILE, "master: wait failed for an unexpected error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
      }
      if ((node_index = find_element(nodes, SO_NODES_NUM, terminated_p)) != -1) {
        if (WIFEXITED(status)) {
          if (WEXITSTATUS(status) == EXIT_FAILURE) {
            fprintf(ERR_FILE, "Node %d encountered an error. Stopping simulation\n", terminated_p);
            cleanup(users, nodes, shm_id, sem_id, queue_id);
            free(nof_transactions);
            exit(EXIT_FAILURE);
          }
          else {
            fprintf(ERR_FILE, "master: node %d terminated with the unexpected status %d while sim was ongoing. Check the code.\n", terminated_p, WEXITSTATUS(status));
            cleanup(users, nodes, shm_id, sem_id, queue_id);
            exit(EXIT_FAILURE);
          }
        }
      }
      else {
        inactive_users++;
      }
    }
    alarm(0);
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
        cleanup(users, nodes, shm_id, sem_id, queue_id);
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

    ending_reason = -1;
    if (simulation_seconds == SO_SIM_SEC) {
      ending_reason = SIM_END_SEC;
    }
    else if (SO_USERS_NUM == 0) {
      ending_reason = SIM_END_USR;
    }
    else if (book->cursor == SO_REGISTRY_SIZE) {
      ending_reason = SIM_END_SIZ;
    }

    periodical_update(users, user_budget, node_budget);
    summary_print(ending_reason, users, user_budget, nodes, node_budget, nof_transactions);
    free(nof_transactions);
  }

  free_list(users);
  free_list(nodes);

  if (shmctl(shm_id, 0, IPC_RMID) == -1) {
    fprintf(ERR_FILE, "master: error in shmctl while removing the shm with id %d. Please check ipcs and remove it.\n", shm_id);
    exit(EXIT_FAILURE);
  }
  if (shmctl(shm_nodes, 0, IPC_RMID) == -1) {
    fprintf(ERR_FILE, "master: error in shmctl while removing the shm with id %d. Please check ipcs and remove it.\n", shm_id);
    exit(EXIT_FAILURE);
  }

  if (semctl(sem_id, 0, IPC_RMID) == -1) {
    fprintf(ERR_FILE, "master: could not free sem %d, please check ipcs and remove it.\n", sem_id);
    exit(EXIT_FAILURE);
  }
  fprintf(LOG_FILE, "Completed simulation. Exiting...\n");
  exit(EXIT_SUCCESS);
}

