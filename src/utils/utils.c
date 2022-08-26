#include "utils.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

void sleep_random_from_range(int min, int max)
{
  int nsec;
  struct timespec sleep_time;
  struct timespec time_remaining;
  srand(clock());

  nsec = rand() % (max - min + 1) + min;
  sleep_time.tv_sec = nsec / 1000000000;
  sleep_time.tv_nsec = nsec;

  while (clock_nanosleep(CLOCK_REALTIME, 0, &sleep_time, &time_remaining) < 0) {
    sleep_time.tv_sec = time_remaining.tv_sec;
    sleep_time.tv_nsec = time_remaining.tv_nsec;
  }
}

int* init_list(int size) {
  int* new = calloc(size, sizeof(int));
  if (new != NULL)
    bzero(new, sizeof(int) * size);

  return new;
}

int random_element(int* list, int size) {
  int random_el = 0, found = 0, dim = size;
  int* cpy = malloc(sizeof(int) * size);
  if (cpy == NULL) {
    return -1;
  }

  {
    register i = 0;
    while (i < dim) {
      cpy[i] = list[i];
      i++;
    }
  }

  /* ogni volta che il ciclo trova un processo terminato, lo separa dai processi attivi, spostando in fondo alla lista.
  In questo modo riduce il sottoarray da cui cercare i processi attivi */
  do {
    int r_index = 0;
    srand(clock());
    r_index = (rand() % dim);
    if (r_index >= 0 && r_index < dim) {
      found = 1; /* imposto found a true */
      random_el = cpy[r_index];
      if (getpid() == random_el || (kill(random_el, 0) == -1 && errno == ESRCH)) {
        found = 0; /* l'elemento corrente non può essere scelto */
        dim--;
        cpy[r_index] = cpy[dim];
        cpy[dim] = random_el;
      }
    }
  } while (dim > 0 && !found); /* evita di estrarre lo stesso processo in cui ci troviamo o di trovare un utente terminato*/

  free(cpy);
  return dim > 0 ? random_el : -1;
}

/* return the index of the first element equal to x, -1 otherwise */
int find_element(int* l, int size, int x) {
  int index = -1;
  register int i = 0;
  while (i < size && index == -1) {
    if (l[i] == x) {
      index = i;
    }
    i++;
  }

  return index;
}
