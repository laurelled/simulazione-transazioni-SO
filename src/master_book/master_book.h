#ifndef _TRANSACTION_H_
#define _TRANSACTION_H_

#define SO_BLOCK_SIZE 3

typedef struct
{
  long timestamp;
  int sender;
  int receiver;
  int quantita;
  int reward;
} transaction;

struct master_book {
  int* size;
  transaction* blocks;
};

struct msg {
  long mtype;
  transaction mtext;
};


void* attach_shm_memory(int shm_id);
void new_transaction(transaction* new, int sender, int reciever, int quantita, int reward);
void print_transaction(transaction t);

#endif