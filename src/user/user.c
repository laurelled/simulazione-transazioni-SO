#include "../utils/utils.h"
#include "../master_book/master_book.h"
#include "../constants/constants.h"
#include "../node/node.h"
#include "../ipc/ipc.h"

#include <sys/shm.h>
#include <string.h>

static int block_reached;


static int bilancio_corrente;
static int cont_try;

static struct nodes_ds nodes;
static struct master_book book;
static int* users;

volatile sig_atomic_t refused_flag;

static void cleanup() {}

static int calcola_bilancio(int, struct master_book, int*);
static transaction generate_and_send_transaction(struct nodes_ds nodes, int* users, struct master_book book);
static sigset_t reserve_bilancio_corrente(void);
static void release_bilancio_corrente(void);

static void sig_handler(int signal) {
  switch (signal) {
  case SIGUSR1:
    /*transazione rifiutata, recupero del credito, incremento del count_try*/
    ++refused_flag;
    break;
  case SIGUSR2:
    /*generazione di una transazione alla ricezione del segnale SIGUSR2*/
    fprintf(stderr, "u%d: recieved SIGUSR2, generation of transaction in progress...\n", getpid());
    if (bilancio_corrente >= 2) {
      char* str;
      transaction sent = generate_and_send_transaction(nodes, users, book);
      fprintf(stderr, "u%d: successfully generated transaction %s", getpid(), (str = print_transaction(sent)));
      free(str);
    }
    else {
      fprintf(stderr, "u%d: actual balance is < 2 (%d), cannot send transaction\n", getpid(), bilancio_corrente);
    }
    break;
  case SIGTERM:
    /*segnale per la terminazione dell'esecuzione*/
    exit(EXIT_SUCCESS);
    break;
  case SIGSEGV:
    fprintf(stderr, "u%d: recieved a SIGSEGV, stopping simulation.\n", getpid());
    exit(EXIT_FAILURE);
    break;
  default:
    fprintf(stderr, "user %d: an unexpected signal was recieved.\n", getpid());
    break;
  }
}

void init_user(int shm_users_array_id, int shm_nodes_array_id, int shm_nodes_size, int shm_book_id, int shm_book_size_id)
{
  bilancio_corrente = SO_BUDGET_INIT;

  users = shmat(shm_users_array_id, NULL, SHM_RDONLY);
  TEST_ERROR_AND_FAIL;
  bzero(&nodes, sizeof(struct nodes_ds));
  nodes.array = shmat(shm_nodes_array_id, NULL, SHM_RDONLY);
  TEST_ERROR_AND_FAIL;
  nodes.size_ptr = shmat(shm_nodes_size, NULL, SHM_RDONLY);
  TEST_ERROR_AND_FAIL;
  book.blocks = shmat(shm_book_id, NULL, SHM_RDONLY);
  TEST_ERROR_AND_FAIL;
  book.size = shmat(shm_book_size_id, NULL, SHM_RDONLY);
  TEST_ERROR_AND_FAIL;

  {
    struct sigaction sa;
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR2);
    sa.sa_flags = 0;
    sa.sa_mask = mask;
    sa.sa_handler = sig_handler;
    sigaction(SIGUSR1, &sa, NULL);
    TEST_ERROR_AND_FAIL;

    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sa.sa_mask = mask;

    sigaction(SIGUSR2, &sa, NULL);
    TEST_ERROR_AND_FAIL;
    sigaction(SIGTERM, &sa, NULL);
    TEST_ERROR_AND_FAIL;
    sigaction(SIGSEGV, &sa, NULL);
    TEST_ERROR_AND_FAIL;
  }

  while (cont_try < SO_RETRY)
  {
    if (refused_flag) {
      int transaction_q = 0, total;
      struct msg incoming;
      transaction refused_t;
      errno = 0;

      refused_flag = 0;
      transaction_q = msgget(REFUSE_Q_KEY, 0);
      TEST_ERROR_AND_FAIL;

      msgrcv(transaction_q, &incoming, sizeof(struct msg) - sizeof(long), getpid(), IPC_NOWAIT);
      if (errno != ENOMSG) {
        TEST_ERROR_AND_FAIL;
      }
      else {
        fprintf(stderr, "u%d: no msg was found with my pid type\n", getpid());
      }

      refused_t = incoming.mtext;
      total = refused_t.quantita + refused_t.reward;

      /* protect update of bilancio_corrente from SIGUSR2 */
      reserve_bilancio_corrente();
      bilancio_corrente += total;
      release_bilancio_corrente();
    }
    bilancio_corrente = calcola_bilancio(bilancio_corrente, book, &block_reached);
    /* protect update of bilancio_corrente from SIGUSR2 */
    reserve_bilancio_corrente();
    if (bilancio_corrente >= 2) {
      generate_and_send_transaction(nodes, users, book);
    }
    else {
      ++cont_try;
    }
    release_bilancio_corrente();

    /*tempo di attesa dopo l'invio di una transazione*/
    sleep_random_from_range(SO_MIN_TRANS_GEN_NSEC, SO_MAX_TRANS_GEN_NSEC);
  }

  exit(EXIT_SUCCESS);
}

static sigset_t reserve_bilancio_corrente() {
  int sblock = SIGUSR2;
  return sig_block(&sblock, 1);
}

static void release_bilancio_corrente() {
  int sunblock = SIGUSR2;
  sig_unblock(&sunblock, 1);
}

static int calcola_bilancio(int bilancio, struct master_book book, int* block_reached)
{
  int i = *block_reached;
  int size = (*book.size) * SO_BLOCK_SIZE;
  while (i < size) {
    transaction t = book.blocks[i];
    if (t.receiver == getpid()) {
      /* protect update of bilancio_corrente from SIGUSR2 */
      reserve_bilancio_corrente();
      bilancio += t.quantita;
      release_bilancio_corrente();
    }
    i++;
  }
  *block_reached = i;
  return bilancio;
}

static transaction generate_and_send_transaction(struct nodes_ds nodes, int* users, struct master_book book) {
  int size = 0, queue_id, cifra_utente, random_user, random_node, total_quantity, reward;
  transaction t;
  struct msg message;
  /* estrazione di un destinatario casuale*/
  random_user = random_element(users, SO_USERS_NUM);
  if (random_user == -1) {
    printf("init_user u%d:  all other users have terminated. Ending successfully.\n", getpid());
    exit(EXIT_SUCCESS);
  }
  /* estrazione di un nodo casuale*/
  size = *(nodes.size_ptr);
  random_node = random_element(nodes.array, size);
  if (random_node == -1) {
    fprintf(stderr, "init_user u%d: all nodes have terminated, cannot send transaction.\n", getpid());
    exit(EXIT_FAILURE);
  }

  /* generazione di un importo da inviare*/
  srand(getpid() + clock());
  total_quantity = (rand() % (bilancio_corrente - 2 + 1)) + 2;
  reward = total_quantity * SO_REWARD / 100;
  if (reward == 0)
    reward = 1;
  cifra_utente = total_quantity - reward;


  /* creazione di una transazione e invio di tale al nodo generato*/
  new_transaction(&t, getpid(), random_user, cifra_utente, reward);
  /* system V message queue */
  /* ricerca della coda di messaggi del nodo random */
  if ((queue_id = msgget(random_node, 0)) == -1) {
    fprintf(stderr, "init_user u%d: cannot retrieve message queue of node %d (%s)\n", getpid(), random_node, strerror(errno));
    CHILD_STOP_SIMULATION;
    exit(EXIT_FAILURE);
  }
  message.mtype = 1;
  message.mtext = t;
  if (msgsnd(queue_id, &message, sizeof(struct msg) - sizeof(long), IPC_NOWAIT) == -1) {
    if (errno != EAGAIN) {
      fprintf(stderr, "init_user u%d: recieved an unexpected error while sending transaction at queue with id %d of node %d: %s.\n", getpid(), queue_id, random_node, strerror(errno));
      CHILD_STOP_SIMULATION;
      exit(EXIT_FAILURE);
    }
    else {
      transaction failed;
      bzero(&failed, sizeof(transaction));
      ++cont_try;
      return failed;
    }
  }
  else {
    cont_try = 0;
    bilancio_corrente -= total_quantity;
    kill(random_node, SIGUSR1);
    return t;
  }
}