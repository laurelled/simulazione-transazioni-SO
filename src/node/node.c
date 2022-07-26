#include <signal.h>
#include <stdlib.h>
#include "../constants/retrieve_constant.h"
#include "../transaction.h"

static int SO_TP_SIZE = retrieve_constant("SO_TP_SIZE");
static int SO_BLOCK_SIZE = retrieve_constant("SO_BLOCK_SIZE");

static transaction *transaction_pool[SO_TP_SIZE];
static int nof_transaction;

void shift_pool()
{
  register int i = 0;
  while (i < SO_BLOCK_SIZE - 1)
  {
    transaction_pool[i] = transaction_pool[i + SO_BLOCK_SIZE - 1];
  }
}

void free_pool()
{
  register int i = 0;
  while (i < SO_TP_SIZE)
  {
    free(transaction_pool[i++]);
  }
}

transaction *extract_block()
{
  transaction *block = calloc(SO_BLOCK_SIZE, sizeof(transaction));
  if (block == NULL)
    exit(EXIT_FAILURE);
  register int i = 0;
  while (i < SO_BLOCK_SIZE - 1)
  {
    // TODO Check transazione non è presente nel libro mastro
    block[i] = transaction_pool[i];
    i++;
  }

  shift_pool();

  transaction *node_transaction = malloc(sizeof(node_transaction));
  block[SO_BLOCK_SIZE] = node_transaction;
  return block;
}
/*
    CODICE PER SLEEP DEL NODO (FORSE)
    long maxT = retrieve_constant("SO_MAX_TRANS_PROC_NSEC");
    long minT = retrieve_constant("SO_MIN_TRANS_PROC_NSEC");
    sleep(rand() % (max - min + 1) + min);
*/

void generate_node(/* libro mastro */)
{
  while (1)
  {
    transaction *block = extract_block();

    free(block);
  }
}