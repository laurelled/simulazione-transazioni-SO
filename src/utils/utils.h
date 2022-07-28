#ifndef _UTILS_H_
#define _UTILS_H_
#define _GNU_SOURCE
#include <stdio.h>

#define LOG_FILE stdout
#define ERR_FILE stderr
int retrieve_constant(const char*);
void sleep_random_from_range(int min, int max);
#endif