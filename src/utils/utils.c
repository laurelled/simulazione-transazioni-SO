#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <string.h>
#include <sys/signal.h>
#define _GNU_SOURCE

extern char** environ;

int retrieve_constant(const char* name)
{
  const char* delim = "=";
  int val = 0;
  char* stringa = *environ;
  while (stringa != NULL && strstr(stringa, name) == NULL)
  {
    stringa = *environ++;
  }
  if (stringa != NULL)
  {
    char* token = strtok(stringa, delim);
    token = strtok(NULL, delim);
    if (token != NULL)
      val = atoi(token);
  }
  return val;
}

void sleep_random_from_range(long min, long max)
{
  long nsec;
  srand(clock());

  nsec = rand() % (max - min + 1) + min;
  struct timespec sleep_t = { nsec / 1000000000, nsec };

  nanosleep(&sleep_t, NULL);
}