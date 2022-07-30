#ifndef _LIST_H_
#define _LIST_H_
#include <sys/types.h>


pid_t* init_list(int size);
void free_list(pid_t* l);
int random_element(pid_t* l, int size);
int find_element(pid_t* l, int size, pid_t pid);

#endif