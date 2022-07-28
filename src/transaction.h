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

void new_transaction(transaction* new, int sender, int reciever, int quantita, int reward);
void print_transaction(transaction t);

#endif