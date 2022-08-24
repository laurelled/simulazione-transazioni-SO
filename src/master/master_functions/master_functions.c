/*
#include "../../utils/utils.h"
#include "../load_constants/load_constants.h"
#include "../ipc_functions/ipc_functions.h"
#include "../master_book/master_book.h"
#include "master_functions/master_functions.h"
#include "../node/node.h"
#include "../user/user.h"
#include "master.h"

void periodical_update(int* users, int* user_budget, int* node_budget) {
  int i = block_reached;
  int size = *book.size;
  while (i < size) {
    transaction* ptr = book.blocks[i++];
    int j = 0;
    int index;
    while (j++ < SO_BLOCK_SIZE - 1) {
      int index;
      if ((index = find_element(users, SO_USERS_NUM, ptr->sender)) != -1) {
        user_budget[index] -= (ptr->quantita + ptr->reward);
        index = find_element(users, SO_USERS_NUM, ptr->receiver);
        user_budget[index] += ptr->quantita;
      }
      ptr++;
    }
    index = find_element(nodes.array, *nodes.size, ptr->receiver);
    node_budget[index] += ptr->quantita;
  }
  block_reached = i;
}
*/