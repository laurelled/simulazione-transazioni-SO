#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

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
  srand(clock());
  long nsec = rand() % (max - min + 1) + min;

  struct timespec sleep_t = { nsec / 1000000000, nsec };
  nanosleep(&sleep_t, NULL);
}