#ifndef _UTILS_H_
#define _UTILS_H_
#define _POSIX_SOURCE
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

#define TEST_ERROR    if (errno) {fprintf(ERR_FILE, \
                       "%s:%d: PID=%5d: Error %d (%s)\n",\
                       __FILE__,\
                       __LINE__,\
                       getpid(),\
                       errno,\
                       strerror(errno));}

#define LOG_FILE stdout
#define ERR_FILE stderr

void sleep_random_from_range(int min, int max);
#endif