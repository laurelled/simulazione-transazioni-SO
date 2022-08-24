#ifndef _LIST_H_
#define _LIST_H_

#include "../master_book/master_book.h"


int* init_list(int size);
void free_list(int* l);
int random_element(int* l, int size);
int* expand_list(int* l, int old_size, int new_size);
int find_element(int* l, int size, int pid);
int refuse_transaction(transaction transaction);

#endif