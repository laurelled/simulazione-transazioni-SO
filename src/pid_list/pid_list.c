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
  int terminated;

  register i = 0;
  while (i < size) {
    list[i] = cpy[i];
    i++;
  }

  /* ogni volta che il ciclo trova un processo terminato, lo separa dai processi attivi, spostando in fondo alla lista.
  In questo modo riduce il sottoarray da cui cercare i processi attivi */
  do {
    int r_index = 0;
    terminated = 0; /* imposto terminated a false */
    r_index = (rand() % (size + 1));
    random_el = cpy[r_index];
    if (kill(random_el, 0) == -1 && errno == ESRCH) {
      pid_t temp = random_el;
      terminated = 1;

      random_el = cpy[size - 1];
      cpy[size - 1] = temp;
      size--;
    }
  } while (getpid() == random_el && size >= 0 && !terminated); /* evita di estrarre lo stesso processo in cui ci troviamo o di trovare un utente terminato*/

  free(cpy);
  return size >= 0 ? random_el : -1;
}

int list_contains(pid_t* l, int size, pid_t pid) {
  pid_t* ptr = l;
  int found = 0;
  while (--size > 0 && !found) {
    if (ptr[size] == pid) {
      found = 1;
    }
  }

  return found;
}