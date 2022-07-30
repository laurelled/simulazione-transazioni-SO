#ifndef _TRANSACTION_H_
#define _TRANSACTION_H_

#define SELF_RECIEVER -1
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
  unsigned int cursor;
  transaction** blocks;
};


struct master_book* get_master_book(int shm_id);
void new_transaction(transaction* new, int sender, int reciever, int quantita, int reward);
void print_transaction(transaction t);

#endif