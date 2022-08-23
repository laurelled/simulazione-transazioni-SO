#define _GNU_SOURCE
#include "master_book.h"
#include "../utils/utils.h"
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
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

void print_transaction(transaction t) {
  if (t.sender != -1)
    fprintf(LOG_FILE, "TS %ld, u%d -> u%d, %d$, taxes: %d$\n", t.timestamp, t.sender, t.receiver, t.quantita, t.reward);
  else
    fprintf(LOG_FILE, "TS %ld, n%d, %d$\n", t.timestamp, t.receiver, t.quantita);

}
