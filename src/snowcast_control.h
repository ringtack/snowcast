#ifndef __SNOWCAST_CONTROL__
#define __SNOWCAST_CONTROL__

#include "./util/protocol.h"
#include "./util/util.h"
#include <ctype.h>

#define POLLFD(new_fd)                                                         \
  (struct pollfd) { .fd = new_fd, .events = POLLIN }

/**
 * Control structure for the ... snowcast control client. Responsible for
 * synchronizing inputs from command line and the server.
 */
typedef struct {
  pthread_mutex_t control_mtx; // synchronize poll calls
  pthread_cond_t control_cond; // synchronize poll calls
  int num_events;              // record num pending
  int stopped;                 // record if client should stop
  struct pollfd pfds[2];       // poll for `stdin` and `server_fd`
} snowcast_control_t;

/**
 * Initializes a snowcast control structure to the provided server socket.
 *
 * Inputs:
 * - int server_fd: the socket of the server
 *
 * Returns:
 * - 0 on success, -1 on failure
 */
int init_snowcast_control(snowcast_control_t *sc, int server_fd);

/**
 * Frees any allocated space from a snowcast control structure.
 */
void destroy_snowcast_control(snowcast_control_t *sc);

/**
 * Check if snowcast control should be stopped.
 */
int check_stopped(snowcast_control_t *sc);

/**
 * Toggle stopped flag in snowcast control.
 */
void toggle_stopped(snowcast_control_t *sc);

/**
 * Helper functions to lock/unlock the control structure.
 */
void lock_snowcast_control(snowcast_control_t *sc);
void unlock_snowcast_control(snowcast_control_t *sc);

/**
 * Cleanup handler for unlocking a mutex in case we get cancelled.
 */
void pthread_unlock_cleanup_handler(void *arg);

/**
 * Helper functions to change values. Returns the old value.
 */
int atomic_incr(int *val, pthread_mutex_t *mtx);
int atomic_decr(int *val, pthread_mutex_t *mtx);

/**
 * Processes user input.
 */
void *process_input();

/**
 * Processes server reply.
 */
void *process_reply();

#endif
