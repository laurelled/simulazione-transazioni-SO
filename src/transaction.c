#include "transaction.h"
#include <time.h>
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

transaction* new_transaction(int sender, int reciever, int quantita, int reward)
{
  transaction* t = (transaction*)malloc(sizeof(transaction));
  t->timestamp = calculate_time();
  t->sender = sender;
  t->receiver = reciever;
  t->quantita = quantita;
  t->reward = reward;

  return t;
}