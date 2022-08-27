#include "../constants/constants.h"
#include "master_utils.h"
#include <signal.h>
#include <stdio.h>
#include <errno.h>

#define MAX_USERS_TO_PRINT 20

extern int SO_USERS_NUM;

int periodical_update(int block_reached, struct users_ds users, struct nodes_ds nodes, struct master_book book) {
  int index;
  transaction t;
  int size = *(book.size) * SO_BLOCK_SIZE;

  while (block_reached < size) {
    t = book.blocks[block_reached];
    if (t.sender == SELF_SENDER) {
      int nodes_size = *(nodes.size_ptr);
      int index = find_element(nodes.array, nodes_size, t.receiver);
      nodes.budgets[index] += t.quantita;
    }
    else {
      int total_quantity = t.quantita + t.reward;
      index = find_element(users.array, SO_USERS_NUM, t.sender);
      users.budgets[index] -= total_quantity;
      index = find_element(users.array, SO_USERS_NUM, t.receiver);
      users.budgets[index] += t.quantita;
    }
    ++block_reached;
  }

  return block_reached;
}

void stop_simulation(int* users, struct nodes_ds nodes) {
  int size;
  int i = 0;
  int child = 0;

  while ((child = users[i]) != 0 && i < SO_USERS_NUM) {
    i++;
    kill(child, SIGTERM);
  }
  i = 0;
  size = *(nodes.size_ptr);
  while ((child = nodes.array[i]) != 0 && i < size) {
    i++;
    kill(child, SIGTERM);
  }

  /* resetting errno as some kill might have failed */
  errno = 0;
}



void periodical_print(struct users_ds users, struct nodes_ds nodes) {
  int i = 0;
  printf("NUMBER OF ACTIVE USERS %d | NUMBER OF ACTIVE NODES %d\n", (SO_USERS_NUM - users.inactive_count), *(nodes.size_ptr));
  /* budget di ogni processo utente (inclusi quelli terminati prematuramente)*/
  printf("USERS (%d) BUDGETS\n", SO_USERS_NUM);
  if (SO_USERS_NUM <= MAX_USERS_TO_PRINT) {
    while (i < SO_USERS_NUM) {
      printf("USER u%d : %d$\n", users.array[i], users.budgets[i]);
      i++;
    }
  }
  else {
    int min_i = 0, max_i = 0;
    printf("[!] users count is too high to display all budgets [!]\n");
    i = 1;
    while (i < SO_USERS_NUM) {
      if (users.budgets[i] < users.budgets[min_i]) {
        min_i = i;
      }
      if (users.budgets[i] > users.budgets[max_i]) {
        max_i = i;
      }
      i++;
    }

    printf("HIGHEST BUDGET: USER u%d : %d$\n", users.array[max_i], users.budgets[max_i]);
    printf("LOWEST BUDGET: USER u%d : %d$\n", users.array[min_i], users.budgets[min_i]);
  }

  {
    int size = *(nodes.size_ptr);
    int i = 0;
    /* budget di ogni processo nodo */
    printf("NODES (%d) BUDGETS\n", *(nodes.size_ptr));
    while (i < size) {
      printf("NODE n%d : %d$\n", nodes.array[i], nodes.budgets[i]);
      i++;
    }
  }
}

void summary_print(int ending_reason, struct users_ds users, struct nodes_ds nodes, int book_size) {
  int i = 0;

  switch (ending_reason)
  {
  case SIM_END_SEC:
    printf("[!] Simulation ending reason: TIME LIMIT REACHED [!]\n\n");
    break;
  case SIM_END_SIZ:
    printf("[!] Simulation ending reason: MASTER BOOK SIZE EXCEEDED [!]\n\n");
    break;
  case SIM_END_USR:
    printf("[!] Simulation ending reason: ALL USERS TERMINATED [!]\n\n");
    break;
  default:
    printf("[!] Simulation ending reason: UNEXPECTED ERRORS [!]\n\n");
    break;
  }
  /* bilancio di ogni processo utente, compresi quelli che sono terminati prematuramente */
  /*printf("USERS BUDGETS\n");
  while (i < SO_USERS_NUM) {
    printf("USER u%d : %d$\n", users.array[i], users.budgets[i]);
    i++;
  }*/
  /* bilancio di ogni processo nodo */
  i = 0;
  printf("NODES (%d) BUDGETS\n", *(nodes.size_ptr));
  while (i < *nodes.size_ptr) {
    printf("NODE n%d : %d$\n", nodes.array[i], nodes.budgets[i]);
    i++;
  }
  printf("NUMBER OF INACTIVE USERS: %d\n", users.inactive_count);
  printf("NUMBER OF TRANSACTION BLOCK WRITTEN INTO THE MASTER BOOK: %d\n\n\n", book_size);

  printf("Number of transactions left per node:\n");
  i = 0;
  while (i < *nodes.size_ptr) {
    printf("NODE %d: %d transactions left\n", nodes.array[i], nodes.transactions_left[i]);
    i++;
  }
}