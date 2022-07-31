#include "utils.h"

#include <stdlib.h>
#include <time.h>

void sleep_random_from_range(int min, int max)
{
  int nsec;
  struct timespec sleep_time;
  struct timespec time_remaining;
  srand(clock());

  nsec = rand() % (max - min + 1) + min;
  sleep_time.tv_sec = nsec / 1000000000;
  sleep_time.tv_nsec = nsec;

  while(clock_nanosleep(CLOCK_REALTIME, 0, &sleep_time, &time_remaining) < 0){
    sleep_time.tv_sec = time_remaining.tv_sec;
    sleep_time.tv_nsec = time_remaining.tv_nsec;
  }
}