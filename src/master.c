#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils/utils.h"
#include "load_constants/load_constants.h"

#define MAX_USERS_TO_PRINT 10
#define SO_REGISTRY_SIZE 10

extern int SO_USERS_NUM;
extern int SO_NODES_NUM;
extern int SO_SIM_SEC;

static int active_users = 0;
static int active_nodes = 0;

static int* users;
static int* nodes;

void periodical_print() {
  fprintf(LOG_FILE, "ACTIVE USERS: %d | ACTIVE NODES: %d\n\n", SO_USERS_NUM, SO_NODES_NUM);
  if (SO_NODES_NUM < MAX_USERS_TO_PRINT) {

  }
  else {

  }
}

void init_master() {
  active_users = SO_USERS_NUM;
  active_nodes = SO_NODES_NUM;

  users = (int*)calloc(SO_USERS_NUM, sizeof(int));
  nodes = (int*)calloc(SO_NODES_NUM, sizeof(int));
}

void stop_simulation() {
  int i = 0;
  while (i < active_users) {
    kill(users[i++], SIGTERM);
  }
  i = 0;
  while (i < active_nodes) {
    kill(nodes[i++], SIGTERM);
  }

  free(users);
  free(nodes);
}

void summary_print() {
  //TODO: bilancio di ogni processo utente, compresi quelli che sono terminati prematuramente
  // TODO: bilancio di ogni processo nodo
  fprintf(LOG_FILE, "NUMBER OF INACTIVE USERS: %d\n", SO_USERS_NUM - active_users);
  fprintf(LOG_FILE, "NUMBER OF TRANSACTION BLOCK WRITTEN INTO THE LEDGER: %d\n", SO_REGISTRY_SIZE /* - size of the ledger*/);
  //TODO: per ogni processo nodo, numero di transazioni ancora presenti nella transaction pool. Forse farlo ritornare come status?


}

int main() {
  init_master();

  int i = 0;
  while (i < SO_SIM_SEC && active_users > 0 /* && actual_capacity < libromastro->capacity */) {
    sleep(1);
    i++;
    periodical_print();
  }

  stop_simulation();
  fprintf(LOG_FILE, "End simulation reason: ");
  if (i == SO_SIM_SEC) {
    fprintf(LOG_FILE, "SIMULATION TIME RUN OFF\n");
  }
  else if (active_users == 0) {
    fprintf(LOG_FILE, "ACTIVE USERS REACHED 0\n");
  }/*else if(actual_capacity == libromastro->capacity) {
    fprintf(LOG_FILE, "LEDGER CAPACITY REACHED MAXIMUM CAPACITY\n");
  }*/
  summary_print();

  //TODO: free_ledger()


  exit(EXIT_SUCCESS);
}