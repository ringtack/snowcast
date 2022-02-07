#ifndef __SYNC_LIST_H__
#define __SYNC_LIST_H__

#include "list.h"
#include <pthread.h>

/**
 * Wrapper for the doubly linked list implementation. Synchronizes access
 * through a coarse-grained lock on the list head.
 */

typedef struct {
  list_t sync_list;
  size_t size;
  pthread_mutex_t mtx;
} sync_list_t;

/*
 * Helpers for empty and size checking.
 */
int sync_list_empty(sync_list_t *sl);
size_t sync_list_size(sync_list_t *sl);

#define sync_list_init(sl)                                                     \
  do {                                                                         \
    list_init(&((sl)->sync_list));                                             \
    (sl)->size = 0;                                                            \
    pthread_mutex_init(&(sl)->mtx, NULL);                                      \
  } while (0)

#define sync_list_destroy(sl) pthread_mutex_destroy(&(sl)->mtx)

#define sync_list_insert_head(sl, link)                                        \
  do {                                                                         \
    pthread_mutex_lock(&(sl)->mtx);                                            \
    list_insert_head(&(sl)->sync_list, (link));                                \
    (sl)->size += 1;                                                           \
    pthread_mutex_unlock(&(sl)->mtx);                                          \
  } while (0)

#define sync_list_insert_tail(sl, link)                                        \
  do {                                                                         \
    pthread_mutex_lock(&(sl)->mtx);                                            \
    list_insert_tail(&(sl)->sync_list, (link));                                \
    (sl)->size += 1;                                                           \
    pthread_mutex_unlock(&(sl)->mtx);                                          \
  } while (0)

#define sync_list_remove(sl, link)                                             \
  do {                                                                         \
    pthread_mutex_lock(&(sl)->mtx);                                            \
    list_remove((link));                                                       \
    (sl)->size -= 1;                                                           \
    pthread_mutex_unlock(&(sl)->mtx);                                          \
  } while (0)

#define sync_list_remove_head(sl)                                              \
  do {                                                                         \
    pthread_mutex_lock(&(sl)->mtx);                                            \
    list_remove_head(link);                                                    \
    (sl)->size -= 1;                                                           \
    pthread_mutex_unlock(&(sl)->mtx);                                          \
  } while (0)

#define sync_list_remove_tail(sl)                                              \
  do {                                                                         \
    pthread_mutex_lock(&(sl)->mtx);                                            \
    list_remove_tail(link);                                                    \
    (sl)->size -= 1;                                                           \
    pthread_mutex_unlock(&(sl)->mtx);                                          \
  } while (0)

#define sync_list_item(sl, link, type, member)                                 \
  do {                                                                         \
    pthread_mutex_lock(&(sl)->mtx);                                            \
    list_item(link, type, member);                                             \
    pthread_mutex_unlock(&(sl)->mtx);                                          \
  } while (0)

#define sync_list_head(sl, link, type, member)                                 \
  do {                                                                         \
    pthread_mutex_lock(&(sl)->mtx);                                            \
    list_head(link, type, member);                                             \
    pthread_mutex_unlock(&(sl)->mtx);                                          \
  } while (0)

#define sync_list_tail(sl, link, type, member)                                 \
  do {                                                                         \
    pthread_mutex_lock(&(sl)->mtx);                                            \
    list_tail(link, type, member);                                             \
    pthread_mutex_unlock(&(sl)->mtx);                                          \
  } while (0)

#define sync_list_iterate_begin(sl, var, type, member)                         \
  do {                                                                         \
    pthread_mutex_lock(&(sl)->mtx);                                            \
  list_iterate_begin(&(sl)->sync_list, var, type, member)

#define sync_list_iterate_end(sl)                                              \
  list_iterate_end();                                                          \
  pthread_mutex_unlock(&(sl)->mtx);                                            \
  }                                                                            \
  while (0)

#endif
