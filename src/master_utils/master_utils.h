#ifndef _MASTER_UTILS_H_
#define _MASTER_UTILS_H_

#include "../node/node.h"
#include "../master_book/master_book.h"

#define SIM_END_SEC 0
#define SIM_END_USR 1
#define SIM_END_SIZ 2

struct users_ds {
  int* array;
  int* budgets;
  int inactive_count;
};

int periodical_update(int block_reached, struct users_ds users, struct nodes_ds nodes, struct master_book book);
void stop_simulation(int* users, struct nodes_ds nodes);
void periodical_print(struct users_ds users, struct nodes_ds nodes);
void summary_print(int ending_reason, struct users_ds users, struct nodes_ds nodes, int book_size);

#endif