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
int SO_REWARD;
int SO_MIN_TRANS_GEN_NSEC;
int SO_MAX_TRANS_GEN_NSEC;
int SO_RETRY;
int SO_HOPS;

static int num_err = 0;

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

    if (val < 0) {
      fprintf(stderr, "%s can not be negative.\n", name);
      ++num_err;
    }
  }
  else {
    fprintf(stderr, "%s could not be found in the environ. Check your .env file or make sure to run 'export $(xargs  < <conf file>)'.\n", name);
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

  if (SO_TP_SIZE == 0) {
    fprintf(stderr, "SO_TP_SIZE should be a positive integer. Check your .env file\n");
    ++num_err;
  }
  if (SO_USERS_NUM <= 1) {
    fprintf(stderr, "SO_USERS_NUM should be greater than 1. Check your .env file\n");
    ++num_err;
  }
  if (SO_REWARD < 0 && SO_REWARD > 100) {
    fprintf(stderr, "SO_REWARD should be an integer value between the interval [0, 100]. Check your .env file\n");
    ++num_err;
  }
  if (SO_HOPS == 0) {
    fprintf(stderr, "SO_HOPS should be a positive integer. Check your .env file\n");
    ++num_err;
  }
  if (SO_NODES_NUM == 0) {
    fprintf(stderr, "SO_NODES_NUM should be a positive integer. Check your .env file\n");
    ++num_err;
  }
  if (SO_BUDGET_INIT == 0) {
    fprintf(stderr, "SO_BUDGET_INIT should be a positive integer. Check your .env file\n");
    ++num_err;
  }
  if (SO_SIM_SEC == 0) {
    fprintf(stderr, "SO_SIM_SEC should be a positive integer. Check your .env file\n");
    ++num_err;
  }


  if (num_err) {
    printf("Number of errors encountered: %d. Please fix your .env file\n", num_err);
    exit(EXIT_FAILURE);
  }
}