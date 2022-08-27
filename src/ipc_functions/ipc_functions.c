#include "ipc_functions.h"
#include "../master_book/master_book.h"
#include "../master/master.h"

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <string.h>
#include <signal.h>

int sem_reserve(int sem_id, int sem_num) {
  struct sembuf sops;
  bzero(&sops, sizeof(struct sembuf));

  sops.sem_num = sem_num;
  sops.sem_op = -1;

  return semop(sem_id, &sops, 1);
}

int sem_release(int sem_id, int sem_num) {
  struct sembuf sops;
  bzero(&sops, sizeof(struct sembuf));

  sops.sem_num = sem_num;
  sops.sem_op = 1;

  return semop(sem_id, &sops, 1);
}

int sem_wait_for_zero(int sem_id, int sem_num) {
  struct sembuf sops;
  bzero(&sops, sizeof(struct sembuf));

  sops.sem_num = sem_num;
  sops.sem_op = 0;

  return semop(sem_id, &sops, 1);
}

sigset_t sig_block(int* signals, int nsig) {
  sigset_t mask;
  sigemptyset(&mask);

  while (--nsig >= 0) {
    int sig = signals[nsig];
    sigaddset(&mask, sig);
  }

  sigprocmask(SIG_BLOCK, &mask, NULL);
  return mask;
}

void sig_unblock(int* signals, int nsig) {
  sigset_t mask;
  sigemptyset(&mask);

  while (--nsig >= 0) {
    int sig = signals[nsig];
    sigaddset(&mask, sig);
  }

  sigprocmask(SIG_UNBLOCK, &mask, NULL);
}