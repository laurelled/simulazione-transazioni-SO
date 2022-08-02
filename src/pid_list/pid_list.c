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
  int* cpy = malloc(sizeof(int) * size);
  int random_el = 0;
  int found;

  register i = 0;
  while (i < size) {
    cpy[i] = list[i];
    i++;
  }

  /* ogni volta che il ciclo trova un processo terminato, lo separa dai processi attivi, spostando in fondo alla lista.
  In questo modo riduce il sottoarray da cui cercare i processi attivi */
  do {
    int r_index = 0;
    srand(clock());
    found = 1; /* imposto found a true */
    r_index = (rand() % size);
    random_el = cpy[r_index];
    if (getpid() == random_el || (kill(random_el, 0) == -1 && errno == ESRCH)) {
      int temp = random_el;
      found = 0;

      cpy[r_index] = cpy[size - 1];
      cpy[size - 1] = temp;
      size--;
    }
  } while (size > 0 && !found); /* evita di estrarre lo stesso processo in cui ci troviamo o di trovare un utente terminato*/

  free(cpy);
  return size > 0 ? random_el : -1;
}

int find_element(int* l, int size, int pid) {
  int* ptr = l;
  int index = -1;
  while (--size > 0 && index != -1) {
    if (ptr[size] == pid) {
      index = size;
    }
  }

  return index;
}