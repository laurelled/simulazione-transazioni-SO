#include "pid_list.h"

#include <stdlib.h>
#include <errno.h>

pid_t* init_list(int size) {
  pid_t* new = malloc(sizeof(pid_t) * size);
  int i = 0;
  while (i < size) {
    new[i++] = 0;
  }

  return new;
}

void free_list(pid_t* l) {
  free(l);
}

int random_element(pid_t* list, int size) {
  pid_t* cpy = malloc(sizeof(pid_t) * size);
  pid_t random_el = 0;
  int found;

  register i = 0;
  while (i < size) {
    list[i] = cpy[i];
    i++;
  }

  /* ogni volta che il ciclo trova un processo terminato, lo separa dai processi attivi, spostando in fondo alla lista.
  In questo modo riduce il sottoarray da cui cercare i processi attivi */
  do {
    int r_index = 0;
    found = 1; /* imposto found a true */
    r_index = (rand() % size);
    random_el = cpy[r_index];
    if (getpid() == random_el || (kill(random_el, 0) == -1 && errno == ESRCH)) {
      pid_t temp = random_el;
      found = 0;

      random_el = cpy[size - 1];
      cpy[size - 1] = temp;
      size--;
    }
  } while (size > 0 && found); /* evita di estrarre lo stesso processo in cui ci troviamo o di trovare un utente terminato*/

  free(cpy);
  return size >= 0 ? random_el : -1;
}

int find_element(pid_t* l, int size, pid_t pid) {
  pid_t* ptr = l;
  int index = -1;
  while (--size > 0 && index != -1) {
    if (ptr[size] == pid) {
      index = size;
    }
  }

  return index;
}