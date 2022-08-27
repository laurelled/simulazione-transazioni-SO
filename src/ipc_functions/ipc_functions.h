#ifndef _IPC_FUNCS_H_
#define _IPC_FUNCS_H_

#include "../utils/utils.h"
#include <signal.h>

#define ID_MEM 1
#define ID_READY_ALL 0

#define IPC_CREATION_FLAGS IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR

int sem_reserve(int sem_id, int sem_num);
int sem_release(int sem_id, int sem_num);
int sem_wait_for_zero(int sem_id, int sem_num);
sigset_t sig_block(int* signals, int nsig);
void sig_unblock(int* signals, int nsig);

#endif