#ifndef _UTILS_H_
#define _UTILS_H_
#define _POSIX_SOURCE
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define CHILD_STOP_SIMULATION kill(getppid(), SIGINT);

#define TEST_ERROR    if (errno) {fprintf(ERR_FILE, \
                       "%s:%d: PID=%5d: Error %d (%s)\n",\
                       __FILE__,\
                       __LINE__,\
                       getpid(),\
                       errno,\
                       strerror(errno));}

#define TEST_ERROR_AND_FAIL    if (errno && errno != ESRCH && errno != EAGAIN) {fprintf(ERR_FILE, \
                       "%s:%d: PID=%5d: Error %d (%s)\n",\
                       __FILE__,\
                       __LINE__,\
                       getpid(),\
                       errno,\
                       strerror(errno)); \
                       cleanup(); \
                       exit(EXIT_FAILURE); }

#define LOG_FILE stdout
#define ERR_FILE stderr

void sleep_random_from_range(int min, int max);
int* init_list(int size);
int random_element(int* l, int size);
int find_element(int* l, int size, int x);

#endif