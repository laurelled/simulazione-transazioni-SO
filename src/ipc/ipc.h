#ifndef _IPC_H_
#define _IPC_H_

#include "../utils/utils.h"
#include "../master_book/master_book.h"
#include <signal.h>

#define ID_SEM_MEM 1
#define ID_SEM_READY_ALL 0
#define REFUSE_Q_KEY getppid() - 1

struct msg {
  long mtype;
  transaction mtext;
};

#define IPC_CREATION_FLAGS IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR

int sem_reserve(int sem_id, int sem_num);
int sem_release(int sem_id, int sem_num);
int sem_wait_for_zero(int sem_id, int sem_num);
sigset_t sig_block(int* signals, int nsig);
void sig_unblock(int* signals, int nsig);
int refuse_transaction(transaction t, int refuse_q);

#endif