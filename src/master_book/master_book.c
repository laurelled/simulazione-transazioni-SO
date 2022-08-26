#define _GNU_SOURCE
#include "master_book.h"
#include "../utils/utils.h"
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <signal.h>
#include <stdlib.h>

/*
  CLOCK_REALTIME
  System-wide clock that measures real (i.e., wall-clock) time.  Setting this clock requires  appropri‐
  ate  privileges.  This clock is affected by discontinuous jumps in the system time (e.g., if the sys‐
  tem administrator manually changes the clock), and by the incremental adjustments performed  by  adj‐
  time(3) and NTP.

  struct timespec {
    time_t   tv_sec;         seconds
    long     tv_nsec;        nanoseconds
  };
*/
static long calculate_time()
{
  struct timespec time;
  clock_gettime(CLOCK_REALTIME, &time);
  return time.tv_nsec;
}

void new_transaction(transaction* new, int sender, int reciever, int quantita, int reward)
{
  new->timestamp = calculate_time();
  new->sender = sender;
  new->receiver = reciever;
  new->quantita = quantita;
  new->reward = reward;
}

int refuse_transaction(transaction t, int user_q) {
  struct msg incoming;
  incoming.mtext = t;
  incoming.mtype = t.sender;
  if (msgsnd(user_q, &incoming, sizeof(struct msg) - sizeof(long), IPC_NOWAIT) == -1) {
    return -1;
  }
  if (kill(t.sender, 0) == -1) {
    return -1;
  }
  kill(t.sender, SIGUSR1);

  return 0;
}

void print_transaction(transaction t) {
  if (t.sender != -1)
    fprintf(LOG_FILE, "TS %ld, u%d -> u%d, %d$, taxes: %d$\n", t.timestamp, t.sender, t.receiver, t.quantita, t.reward);
  else
    fprintf(LOG_FILE, "TS %ld, n%d, %d$\n", t.timestamp, t.receiver, t.quantita);

}

int find_element_in_book(struct master_book book, int limit, transaction x) {
  int found = 0;
  while (limit >= 0 && !found) {
    transaction t = book.blocks[limit];
    if (t.timestamp == x.timestamp && t.sender == x.sender && t.receiver == x.receiver) {
      found = 1;
    }
    else
      limit--;
  }

  return found;
}

