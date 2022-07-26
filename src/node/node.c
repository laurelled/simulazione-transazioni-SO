#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include "utils.h"
#include "transaction.h"

static int SO_BLOCK_SIZE = 0;
static int SO_TP_SIZE = 0;

static transaction** transaction_pool = NULL;
static int nof_transaction = 0;

static void handler_term(int sigTerm) {
  free_pool_block(transaction_pool, 0, SO_TP_SIZE);

  signal(SIGTERM, SIG_DFL);
}

static void init_node()
{
  SO_TP_SIZE = retrieve_constant("SO_TP_SIZE");
  SO_BLOCK_SIZE = retrieve_constant("SO_BLOCK_SIZE");

  if (SO_BLOCK_SIZE >= SO_TP_SIZE) {
    fprintf(LOG_FILE, "init_node: SO_BLOCK_SIZE >= SO_TP_SIZE. Check environmental config.\n");
    exit(EXIT_FAILURE);
  }

  transaction_pool = (transaction**)calloc(SO_TP_SIZE, sizeof(transaction*));
}

static void shift_pool()
{
  register int i = 0;
  while (i < SO_BLOCK_SIZE - 1)
  {
    transaction_pool[i] = transaction_pool[i + SO_BLOCK_SIZE - 1];
  }
}

static void free_pool_block(transaction** pool, int start, int end)
{
  if (pool != NULL) {
    register int i = start;
    while (i < end)
    {
      free(pool[i++]);
    }
  }
}

static transaction** extract_block()
{
  int gain = 0;
  transaction** block = calloc(SO_BLOCK_SIZE, sizeof(transaction));
  if (block == NULL)
    exit(EXIT_FAILURE);
  register int i = 0;
  while (i < SO_BLOCK_SIZE - 1)
  {
    // TODO Check transazione non è presente nel libro mastro
    block[i] = transaction_pool[i];
    gain += block[i]->reward;
    i++;
  }

  shift_pool();

  transaction* node_transaction = new_transaction(getpid(), SELF_RECIEVER, gain, 0);
  block[SO_BLOCK_SIZE] = node_transaction;
  return block;
}
/*
    CODICE PER SLEEP DEL NODO (FORSE)
    long maxT = retrieve_constant("SO_MAX_TRANS_PROC_NSEC");
    long minT = retrieve_constant("SO_MIN_TRANS_PROC_NSEC");
    sleep(rand() % (max - min + 1) + min);
*/

static void simulate_processing() {
  long max_t = retrieve_constant("SO_MAX_TRANS_PROC_NSEC");
  long min_t = retrieve_constant("SO_MIN_TRANS_PROC_NSEC");
  sleep_random_from_range(min_t, max_t);
}

void generate_node(/* libro mastro */)
{
  init_node();
  while (1)
  {
    transaction** block = extract_block();
    // TODO: scrittura nel libro mastro
    simulate_processing();
    free_pool_block(block, 0, SO_BLOCK_SIZE);
    free(block);
  }
}