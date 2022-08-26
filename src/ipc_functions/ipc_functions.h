#ifndef _IPC_FUNCS_H_
#define _IPC_FUNCS_H_

#define ID_MEM 1
#define ID_READY_ALL 0

int sem_reserve(int sem_id, int sem_num);
int sem_release(int sem_id, int sem_num);
int sem_wait_for_zero(int sem_id, int sem_num);

#endif