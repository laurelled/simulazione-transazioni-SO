#ifndef _MASTER_H_
#define _MASTER_H_
#define ID_MEM 1
#define SO_REGISTRY_SIZE 10000
#define ID_READY_ALL 0
#define MSG_Q getppid()-1
struct nodes {
  int* size;
  int* array;
};
#endif