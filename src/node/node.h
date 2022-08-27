#ifndef _NODE_H_
#define _NODE_H_

#define MAX_NODES SO_NODES_NUM * SO_NODES_NUM

struct nodes_ds {
  int* size_ptr;
  int* array;
  int* budgets;
  int* transactions_left;
  int* write_pipes;
};

void init_node(int* friends, int pipe_read, int shm_book_id, int shm_book_size_id);
#endif