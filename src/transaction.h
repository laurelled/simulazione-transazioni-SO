#ifndef _TRANSACTION_H_
#define _TRANSACTION_H_

typedef struct
{
  long timestamp;
  int sender;
  int receiver;
  int quantita;
  int reward;
} transaction;

transaction *new_transaction(int sender, int reciever, int quantita, int reward);

#endif