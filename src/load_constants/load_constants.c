#include "../utils/utils.h"

#include <stdlib.h>
#include <string.h>

extern char** environ;

int SO_TP_SIZE;
int SO_MAX_TRANS_PROC_NSEC;
int SO_MIN_TRANS_PROC_NSEC;
int SO_USERS_NUM;
int SO_NODES_NUM;
int SO_SIM_SEC;
int SO_NUM_FRIENDS;
int SO_BUDGET_INIT;
int SO_USERS_NUM;
int SO_NODES_NUM;
int SO_REWARD;
int SO_MIN_TRANS_GEN_NSEC;
int SO_MAX_TRANS_GEN_NSEC;
int SO_RETRY;
int SO_HOPS;

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

void load_constants() {
  SO_TP_SIZE = retrieve_constant("SO_TP_SIZE");
  SO_MAX_TRANS_PROC_NSEC = retrieve_constant("SO_MAX_TRANS_PROC_NSEC");
  SO_MIN_TRANS_PROC_NSEC = retrieve_constant("SO_MIN_TRANS_PROC_NSEC");
  SO_USERS_NUM = retrieve_constant("SO_USERS_NUM");
  SO_NODES_NUM = retrieve_constant("SO_NODES_NUM");
  SO_SIM_SEC = retrieve_constant("SO_SIM_SEC");
  SO_NUM_FRIENDS = retrieve_constant("SO_NUM_FRIENDS");
  SO_HOPS = retrieve_constant("SO_HOPS");

  SO_BUDGET_INIT = retrieve_constant("SO_BUDGET_INIT");
  SO_REWARD = retrieve_constant("SO_REWARD");
  SO_MIN_TRANS_GEN_NSEC = retrieve_constant("SO_MIN_TRANS_GEN_NSEC");
  SO_MAX_TRANS_GEN_NSEC = retrieve_constant("SO_MAX_TRANS_GEN_NSEC");

  SO_RETRY = retrieve_constant("SO_RETRY");
}