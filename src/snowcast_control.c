#include "snowcast_control.h"

// global control structure
snowcast_control_t sc;

int main(int argc, char *argv[]) {
  if (argc != 4) {
    fprintf(stderr, "Usage: ./snowcast_control <SERVER_NAME> <SERVER_PORT> "
                    "<LISTENER_PORT>\n");
    exit(1);
  }

  const char *hostname = argv[1], *port = argv[2];

  // get server connection; server must be open at this time, since this calls
  // `accept`!
  int server_fd = get_socket(hostname, port, SOCK_STREAM);
  if (server_fd == -1) {
    fprintf(stderr, "Could not connect to server %s:%s.\n", hostname, port);
    exit(1);
  }

  // initialize control structure
  init_snowcast_control(&sc, server_fd);

  // print usage format
  printf("Type in a number to set the station on which we're listening to that "
         "number.\n");
  printf("Type in 'q', Ctrl-D, or Ctrl-C to quit.\n");

  // Send Hello, and await Welcome. If the server sends anything else, report
  // incorrect command and exit.
  uint16_t listener_port = atoi(argv[3]);
  int ret = send_command_msg(server_fd, MESSAGE_HELLO, listener_port);
  if (ret != 0) {
    fprintf(stderr, "Failed to send message to server. Shutting down..\n");
    destroy_snowcast_control(&sc);
    exit(1);
  }

  uint8_t type;
  void *msg = recv_reply_msg(server_fd, &type);
  if (msg == NULL) {
    fprintf(stderr, "Failed to receive reply from server. Shutting down...\n");
    destroy_snowcast_control(&sc);
    exit(1);
  }

  if (type == REPLY_WELCOME) {
    welcome_t *welcome = (welcome_t *)msg;
    printf("Welcome to Snowcast! The server has %d station(s).\n",
           welcome->num_stations);
  }
  // free message; we don't need anymore
  free(msg);
  // if not Welcome, report error and close server socket
  if (type != REPLY_WELCOME) {
    fprintf(stderr, "Server %s:%s sent an invalid reply. Shutting down...\n",
            argv[1], argv[2]);
    destroy_snowcast_control(&sc);
    exit(1);
  }

  printf("> ");
  fflush(stdout);
  // loop until we break out: either server sends invalid request or client
  // quits
  while (!check_stopped(&sc)) {
    // if any pending events to complete, wait
    lock_snowcast_control(&sc);

    // wait until REPL/client handlers are done
    while (sc.num_events > 0) {
      pthread_cond_wait(&sc.control_cond, &sc.control_mtx);
    }
    // quit if stopped
    if (sc.stopped) {
      unlock_snowcast_control(&sc);
      break;
    }

    // push cleanup handler in case of cancel
    pthread_cleanup_push(pthread_unlock_cleanup_handler, &sc.control_mtx);
    // poll indefinitely
    sc.num_events = poll(sc.pfds, 2, -1);
    // unlock mutex
    pthread_cleanup_pop(0);
    unlock_snowcast_control(&sc);

    // if no event (somehow) or an error occurred, go again
    if (sc.num_events <= 0) {
      if (sc.num_events < 0)
        perror("poll_connections: poll");
      continue;
    }

    // if client typed something, spawn a thread to handle
    if (sc.pfds[0].revents & POLLIN) {
      pthread_t th;
      pthread_create(&th, NULL, process_input, NULL);
      pthread_detach(th);
    }

    // if server sent reply, spawn a thread to handle
    if (sc.pfds[1].revents & POLLIN) {
      pthread_t th;
      pthread_create(&th, NULL, process_reply, NULL);
      pthread_detach(th);
    }
  }
  // need to wait, in case threads are not done with processing yet
  lock_snowcast_control(&sc);
  while (sc.num_events > 0)
    pthread_cond_wait(&sc.control_cond, &sc.control_mtx);
  unlock_snowcast_control(&sc);

  // cleanup
  destroy_snowcast_control(&sc);
  return 0;
}

void *process_input() {
  // can use static, since only this function accesses
  static char msg[MAXBUFSIZ];
  memset(msg, 0, sizeof(msg));
  if (fgets(msg, MAXBUFSIZ, stdin)) {
    // if station, parse and send message
    if (isdigit(msg[0])) {
      uint16_t station = atoi(msg);
      int ret = send_command_msg(sc.pfds[1].fd, MESSAGE_SET_STATION, station);
      if (!ret) {
        printf("Waiting for an announce...\n");
      } else {
        // if failed to send, display error and shut down.
        fprintf(stderr, "Failed to send message to server.\n");
        toggle_stopped(&sc);
      }
      // if 'q', quit out
    } else if (msg[0] == 'q') {
      toggle_stopped(&sc);
    }
  } else {
    fprintf(stderr, "Received EOF/error. Shutting down...\n");
    toggle_stopped(&sc);
  }

  // decrement number of events we're waiting on, and if we hit 0, signal to
  // main thread.
  lock_snowcast_control(&sc);
  if (--sc.num_events == 0)
    pthread_cond_signal(&sc.control_cond);
  unlock_snowcast_control(&sc);

  return NULL;
}

void *process_reply() {
  uint8_t type;
  void *msg = recv_reply_msg(sc.pfds[1].fd, &type);
  if (!msg) {
    fprintf(stderr, "Failed to receive reply from server. Shutting down...\n");
    toggle_stopped(&sc);
  } else if (type == REPLY_ANNOUNCE) {
    announce_t *announce = (announce_t *)msg;
    printf("New song announced: %s\n", announce->songname);
    printf("> ");
    fflush(stdout);
  } else if (type == REPLY_INVALID) {
    invalid_command_t *invalid = (invalid_command_t *)msg;
    fprintf(stderr, "INVALID_COMMAND_REPLY: %s\n", invalid->reply_string);
    toggle_stopped(&sc);
  } else {
    fprintf(stderr, "Invalid reply type. Shutting down...\n");
    toggle_stopped(&sc);
  }
  free(msg);

  // decrement number of events we're waiting on, and if we hit 0, signal to
  // main thread.
  lock_snowcast_control(&sc);
  if (--sc.num_events == 0)
    pthread_cond_signal(&sc.control_cond);
  unlock_snowcast_control(&sc);

  return NULL;
}

int init_snowcast_control(snowcast_control_t *sc, int server_fd) {
  int ret = pthread_mutex_init(&sc->control_mtx, NULL);
  if (ret) {
    handle_error_en(ret, "pthread_mutex_create");
    return -1;
  }
  ret = pthread_cond_init(&sc->control_cond, NULL);
  if (ret) {
    handle_error_en(ret, "pthread_cond_create");
    return -1;
  }
  sc->num_events = 0;
  sc->stopped = 0;
  sc->pfds[0] = POLLFD(STDIN_FILENO);
  sc->pfds[1] = POLLFD(server_fd);
  return 0;
}

void destroy_snowcast_control(snowcast_control_t *sc) {
  int ret = pthread_mutex_destroy(&sc->control_mtx);
  if (ret)
    handle_error_en(ret, "pthread_mutex_destroy");

  ret = pthread_cond_destroy(&sc->control_cond);
  if (ret)
    handle_error_en(ret, "pthread_cond_destroy");

  close(sc->pfds[1].fd);
}

int check_stopped(snowcast_control_t *sc) {
  pthread_mutex_lock(&sc->control_mtx);
  int stopped = sc->stopped;
  pthread_mutex_unlock(&sc->control_mtx);
  return stopped;
}

void toggle_stopped(snowcast_control_t *sc) {
  pthread_mutex_lock(&sc->control_mtx);
  sc->stopped = 1;
  pthread_mutex_unlock(&sc->control_mtx);
}

void lock_snowcast_control(snowcast_control_t *sc) {
  pthread_mutex_lock(&sc->control_mtx);
}
void unlock_snowcast_control(snowcast_control_t *sc) {
  pthread_mutex_unlock(&sc->control_mtx);
}

void pthread_unlock_cleanup_handler(void *arg) {
  printf("Polling thread cancelled. Unlocking mutex...\n");
  pthread_mutex_unlock((pthread_mutex_t *)arg);
}

int atomic_incr(int *val, pthread_mutex_t *mtx) {
  pthread_mutex_lock(mtx);
  int old = (*val)++;
  pthread_mutex_unlock(mtx);
  return old;
}

int atomic_decr(int *val, pthread_mutex_t *mtx) {
  pthread_mutex_lock(mtx);
  int old = (*val)--;
  pthread_mutex_unlock(mtx);
  return old;
}
