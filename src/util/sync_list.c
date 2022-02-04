#include "sync_list.h"

int sync_list_empty(sync_list_t *sl) {
  pthread_mutex_lock(&sl->mtx);
  int empty = list_empty(&sl->sync_list);
  pthread_mutex_unlock(&sl->mtx);
  return empty;
}

size_t sync_list_size(sync_list_t *sl) {
  pthread_mutex_lock(&sl->mtx);
  size_t size = sl->size;
  pthread_mutex_unlock(&sl->mtx);
  return size;
}
