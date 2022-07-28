#include "utils.h"

#include <stdlib.h>
#include <time.h>

int sleep_random_from_range(int min, int max)
{
  int nsec;
  struct timespec sleep_time;
  srand(clock());

  nsec = rand() % (max - min + 1) + min;
  sleep_time.tv_sec = nsec / 1000000000;
  sleep_time.tv_nsec = nsec;
  return nanosleep(&sleep_time, NULL);
}