#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern char **environ;

int retrieve_constant(const char *name)
{
  const char *delim = "=";
  int val = 0;
  char *stringa = *environ;
  while (stringa != NULL && strstr(stringa, name) == NULL)
  {
    stringa = *environ++;
  }
  if (stringa != NULL)
  {
    char *token = strtok(stringa, delim);
    token = strtok(NULL, delim);
    val = atoi(token);
  }
  return val;
}
