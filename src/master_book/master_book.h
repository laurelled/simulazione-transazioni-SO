#ifndef _TRANSACTION_H_
#define _TRANSACTION_H_

#define SO_BLOCK_SIZE 10

typedef struct
{
  long timestamp;
  int sender;
  int receiver;
  int quantita;
  int reward;
} transaction;

struct master_book {
  /* dimensione codificata in numero di blocchi di transazioni */
  int* size;
  transaction* blocks;
};

struct msg {
  long mtype;
  transaction mtext;
};


void* attach_shm_memory(int shm_id, int flags);
void new_transaction(transaction* new, int sender, int reciever, int quantita, int reward);
void print_transaction(transaction t);

#endif