#include "pid_list.h"

#include <stdlib.h>
#include <errno.h>

int* init_list(int size) {
  int* new = malloc(sizeof(int) * size);
  if (new != NULL) {
    int i = 0;
    while (i < size) {
      new[i++] = 0;
    }
  }

  return new;
}

int* expand_list(int* l, int old_size, int new_size) {
  int* expanded = realloc(l, new_size);

  if (expanded != NULL) {
    int i = old_size;
    while (i < new_size) {
      expanded[i++] = 0;
    }
  }
  return expanded;
}

void free_list(int* l) {
  free(l);
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
        found = 0;
        dim--;
        cpy[r_index] = cpy[dim];
        cpy[dim] = random_el;
      }
    }
  } while (dim > 0 && !found); /* evita di estrarre lo stesso processo in cui ci troviamo o di trovare un utente terminato*/

  free(cpy);
  return dim > 0 ? random_el : -1;
}

int find_element(int* l, int size, int pid) {
  int* ptr = l;
  int index = -1;
  int i = 0;
  while (i < size && index == -1) {
    if (ptr[i] == pid) {
      index = i;
    }
    i++;
  }

  return index;
}