#include "thread_pool.h"

thread_pool_t *init_thread_pool(size_t num_threads) {
  // validate valid number of threads
  assert(num_threads > 0);

  // allocate space for thread pool
  thread_pool_t *t_pool =
      malloc(sizeof(thread_pool_t) + num_threads * sizeof(pthread_t));
  if (t_pool == NULL) {
    fprintf(stderr, "[init_thread_pool] Failed to malloc thread_pool.\n");
    return NULL;
  }

  // initialize work queue
  list_init(&t_pool->work_queue);

  // set flags and initial thread count
  t_pool->stopped = 0;
  t_pool->num_threads = num_threads;

  // initialize synchronization primitives
  int ret;
  if ((ret = pthread_mutex_init(&t_pool->mtx, NULL)) ||
      (ret = pthread_cond_init(&t_pool->cond, NULL)) ||
      (ret = pthread_cond_init(&t_pool->finished, NULL))) {
    free(t_pool);
    handle_error_en(ret, "init_thread_pool: pthread_{mutex, cond}_init");
  }

  // run worker threads! detach them so we don't have to worry about joining
  for (size_t i = 0; i < num_threads; i++) {
    if ((ret = (pthread_create(&t_pool->workers[i], NULL, work_loop, t_pool) ||
                pthread_detach(t_pool->workers[i])))) {
      // if creating any thread fails, cancel and join all existing threads
      for (size_t j = 0; j < i; j++)
        pthread_cancel(t_pool->workers[j]); // can't really error check this lol
      // free remnants
      free(t_pool);
      handle_error_en(ret, "init_thread_pool: pthread_{create, detach}");
    }
  }
  return t_pool;
}

void wait_thread_pool(thread_pool_t *t_pool) {
  // synchronize access
  pthread_mutex_lock(&t_pool->mtx);

  // wait until no work, or stopped
  while (!list_empty(&t_pool->work_queue) && !t_pool->stopped)
    pthread_cond_wait(&t_pool->finished, &t_pool->mtx);

  pthread_mutex_unlock(&t_pool->mtx);
}

void destroy_thread_pool(thread_pool_t *t_pool) {
  // first, set stopped to true
  pthread_mutex_lock(&t_pool->mtx);
  t_pool->stopped = 1;

  // broadcast to all sleeping threads to wakey wakey
  int ret = pthread_cond_broadcast(&t_pool->cond);

  // wait for all worker threads to finish their stuffs; we need to free up
  // thread pool mutex while we wait
  while (t_pool->num_threads > 0)
    pthread_cond_wait(&t_pool->finished, &t_pool->mtx);

  // destroy any leftover jobs
  job_t *job;
  list_iterate_begin(&t_pool->work_queue, job, job_t, link) {
    destroy_job(job);
  }
  list_iterate_end();

  // unlock, then destroy synchronization primitives
  pthread_mutex_unlock(&t_pool->mtx);
  ret = ret || pthread_mutex_destroy(&t_pool->mtx) ||
        pthread_cond_destroy(&t_pool->cond) ||
        pthread_cond_destroy(&t_pool->finished);

  // free thread pool
  free(t_pool);
  if (ret)
    handle_error_en(ret, "destroy_thread_pool: pthread_{mutex, cond}_destroy");
}

job_t *init_job(thread_func_t work, void *arg) {
  // allocate space, and check if malloc'd successfully
  job_t *job = malloc(sizeof(job_t));
  if (job == NULL) {
    fprintf(stderr, "[init_job] Failed to malloc a job.\n");
    return NULL;
  }

  // initialize list_link, and set fields
  list_link_init(&job->link);
  job->work = work;
  job->arg = arg;

  return job;
}

int add_job(thread_pool_t *t_pool, thread_func_t work, void *arg) {
  // only attempt if the thread pool even exists
  if (t_pool == NULL)
    return 0;
  // synchronize access
  pthread_mutex_lock(&t_pool->mtx);
  // only add job if not stopped already! indicate with value
  int success = 1;
  if (!t_pool->stopped) {
    // create job
    job_t *job = init_job(work, arg);
    if (job == NULL) {
      pthread_mutex_unlock(&t_pool->mtx);
      return -1;
    }
    // insert job to end of queue, and notify waiting threads!
    list_insert_tail(&t_pool->work_queue, &job->link);
    int ret;
    if ((ret = pthread_cond_signal(&t_pool->cond))) {
      fprintf(stderr, "Failed to signal to t_pool->cond.\n");
      pthread_mutex_unlock(&t_pool->mtx);
      return -1;
    }
  } else
    success = 0;
  // otherwise, do nothing
  pthread_mutex_unlock(&t_pool->mtx);
  return success;
}

void destroy_job(job_t *job) {
  // deallocate args (recall args must be dynamically allocated!)
  free(job->arg);
  free(job);
}

void *work_loop(void *arg) {
  thread_pool_t *t_pool = (thread_pool_t *)arg;

  // loop indefinitely for jobs
  while (1) {
    // acquire lock on thread pool
    pthread_mutex_lock(&t_pool->mtx);
    // check if a job is available; otherwise, wait
    while (list_empty(&t_pool->work_queue) && !t_pool->stopped)
      // wait until we have a job
      pthread_cond_wait(&t_pool->cond, &t_pool->mtx);

    // if stopped, exit thread
    if (t_pool->stopped)
      break;

    // pop first job from list
    job_t *job = list_head(&t_pool->work_queue, job_t, link);
    list_remove_head(&t_pool->work_queue);

    // unlock mutex
    pthread_mutex_unlock(&t_pool->mtx);

    // start work!
    job->work(job->arg);

    // destroy when done (recall jobs are dynamically initialized!)
    destroy_job(job);

    // if list is empty, signal that we are done with work for now
    pthread_mutex_lock(&t_pool->mtx);
    if (list_empty(&t_pool->work_queue))
      pthread_cond_signal(&t_pool->finished);
    pthread_mutex_unlock(&t_pool->mtx);
  }

  // if this is last thread, notify to condition variable that we're done!
  if (--t_pool->num_threads == 0)
    pthread_cond_signal(&t_pool->finished);
  pthread_mutex_unlock(&t_pool->mtx);

  return NULL;
}
