#ifndef _USER_H_
#define _USER_H_
#define EARLY_FAILURE 69
#include <sys/types.h>

void init_user(pid_t* users, int shm_nodes_array, int shm_nodes_size, int shm_book_id, int shm_book_size_id);
#endif