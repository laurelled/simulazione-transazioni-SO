#ifndef _TRANSACTION_H_
#define _TRANSACTION_H_

#ifdef CONF1
#define SO_BLOCK_SIZE 100
#define SO_REGISTRY_SIZE 1000
#else
#ifdef CONF2
#define SO_BLOCK_SIZE 10
#define SO_REGISTRY_SIZE 10000
#else
#ifdef CONF3 
#define SO_BLOCK_SIZE 10
#define SO_REGISTRY_SIZE 1000
#else
#define SO_BLOCK_SIZE 10
#define SO_REGISTRY_SIZE 1000
#endif
#endif
#endif

#define SELF_SENDER -1


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


void new_transaction(transaction* new, int sender, int reciever, int quantita, int reward);
char* print_transaction(transaction t);
int find_element_in_book(struct master_book book, int limit, transaction x);

#endif