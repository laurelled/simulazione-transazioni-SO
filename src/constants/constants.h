#ifndef _CONSTANTS_H_
#define _CONSTANTS_H_

#ifdef CONF1
#define SO_BLOCK_SIZE 100
#define SO_REGISTRY_SIZE 1000
#else
#ifdef CONF2
#define SO_BLOCK_SIZE 10
#define SO_REGISTRY_SIZE 10000
#else
#ifdef CONF3 
#define SO_BLOCK_SIZE 10
#define SO_REGISTRY_SIZE 1000
#else
#define SO_BLOCK_SIZE 10
#define SO_REGISTRY_SIZE 1000
#endif
#endif
#endif

extern int SO_TP_SIZE;
extern int SO_MAX_TRANS_PROC_NSEC;
extern int SO_MIN_TRANS_PROC_NSEC;
extern int SO_USERS_NUM;
extern int SO_NODES_NUM;
extern int SO_SIM_SEC;
extern int SO_NUM_FRIENDS;
extern int SO_BUDGET_INIT;
extern int SO_REWARD;
extern int SO_MIN_TRANS_GEN_NSEC;
extern int SO_MAX_TRANS_GEN_NSEC;
extern int SO_RETRY;
extern int SO_HOPS;

void load_constants();

#endif