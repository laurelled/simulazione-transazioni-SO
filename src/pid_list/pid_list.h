#ifndef _LIST_H_
#define _LIST_H_


int* init_list(int size);
void free_list(int* l);
int random_element(int* l, int size);
int* expand_list(int* l, int old_size, int new_size);
int find_element(int* l, int size, int pid);

#endif