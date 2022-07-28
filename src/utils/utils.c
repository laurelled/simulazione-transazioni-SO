#include "utils.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

extern char** environ;

int retrieve_constant(const char* name)
{
  const char* delim = "=";
  int val = 0;
  char** ptr = environ;
  char* stringa = *ptr;
  while (stringa != NULL && strstr(stringa, name) == NULL) {
    stringa = *ptr++;
  }
  if (stringa != NULL)
  {
    char* token = strtok(stringa, delim);
    token = strtok(NULL, delim);
    if (token != NULL)
      val = atoi(token);
  }
  else {
    fprintf(ERR_FILE, "%s could not be found in the environ. Check your .env file or make sure to run 'export $(xargs  < <conf file>)'.\n", name);
    exit(EXIT_FAILURE);
  }

  return val;
}

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