#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__

#include "util.h"

#define handle_error_en(en, msg)                                               \
  do {                                                                         \
    errno = en;                                                                \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

typedef void (*thread_func_t)(void *arg); // define a job to do

typedef struct {
  list_link_t link;   // for linked list purposes
  thread_func_t work; // job
  void *arg;          // argument(s) of the job
} job_t;

typedef struct {
  list_t work_queue;       // work queue; synchronize access with mutex!
  int stopped;             // flag for stopped; 0 -> running, 1 -> stopped
  pthread_mutex_t mtx;     // synchronize access to thread pool
  pthread_cond_t cond;     // allow threads to wait until work appears
  pthread_cond_t finished; // wait for threads to finish work before destroying
  size_t num_threads;      // keep track of number of threads
  pthread_t workers[];     // VLA for worker threads
} thread_pool_t;

/**
 * Creates a thread pool with the specified number of threads.
 *
 * Inputs:
 * - size_t num_threads: the desired number of threads in the pool
 *
 * Returns:
 * - a dynamically allocated thread pool, or NULL if error
 */
thread_pool_t *init_thread_pool(size_t num_threads);

/**
 * Wait until all work is done, or server is stopped.
 *
 * Inputs:
 * - thread_pool_t *t_pool: the thread pool to wait on
 */
void wait_thread_pool(thread_pool_t *t_pool);

/**
 * Destroys the thread pool when finished. Wait for all threads to finish
 * leftover work! Note that THIS DOES NOT FINISH ALL LEFTOVER JOBS ON THE WORK
 * QUEUE; if that is your intention, call wait_thread_pool before destroying.
 *
 * Inputs:
 * - thread_pool_t *t_pool: a dynamically allocated pool
 */
void destroy_thread_pool(thread_pool_t *t_pool);

/**
 * Create a job to run.
 *
 * Inputs:
 * - thread_func_t work: work function to perform
 * - void *arg: a DYNAMICALLY ALLOCATED struct containing arguments to work
 *
 * Returns:
 * - the dynamically allocated job struct.
 */
job_t *init_job(thread_func_t work, void *arg);

/**
 * Adds a job to the thread pool.
 *
 * Inputs:
 * - thread_pool_t *t_pool: the desired thread pool
 * - thread_func_t work: the work to perform
 * - void *arg: the arguments of the work function; MUST BE DYNAMICALLY
 * ALLOCATED!
 *
 * Returns:
 * - 1 if successfully added, 0 if stopped, -1 if error
 */
int add_job(thread_pool_t *t_pool, thread_func_t work, void *arg);

/**
 * Destroys an allocated job.
 *
 * Inputs:
 * - job_t *job: the dynamically allocated job struct. Note that job's args were
 * DYNAMICALLY ALLOCATED TOO, so YOU MUST FREE THESE!
 */
void destroy_job(job_t *job);

/**
 * Work loop for each worker thread; runs indefinitely until stopped.
 *
 * Inputs:
 * - void *arg: casts to thread_pool_t*.
 *
 * Returns:
 * - NULL
 */
void *work_loop(void *arg);

#endif
