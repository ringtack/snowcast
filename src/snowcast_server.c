#include "snowcast_server.h"

// Global control structures
server_control_t server_control;
station_control_t station_control;
client_control_t client_control;

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(
        stderr,
        "Usage: ./snowcast_server <PORT> <FILE1> [<FILE2> [<FILE3> [...]]]\n");
    exit(1);
  }

  /* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
  /* |I|N|I|T|I|A|L|I|Z|A|T|I|O|N| */
  /* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
  // open listener socket
  int listener = get_socket(NULL, argv[1], SOCK_STREAM);
  if (listener == -1)
    exit(1); // get_socket handles error printing

  int ret;
  // Initialize client, server, and station control structs
  // clean up everything on failure
  // TODO: do I actually need the destroy_struct... calls? so tedious........
  if ((ret = init_server_control(&server_control, INIT_NUM_THREADS))) {
    close(listener);
    exit(1);
  }

  size_t num_stations = argc - 2;
  char **songs = argv + 2;
  if ((ret = init_station_control(&station_control, num_stations, songs))) {
    close(listener);
    exit(1);
  }

  if ((ret = init_client_control(&client_control, listener))) {
    close(listener);
    exit(1);
  }

  /* +-+-+-+-+ +-+-+-+-+-+-+-+ */
  /* |P|O|L|L| |C|L|I|E|N|T|S| */
  /* +-+-+-+-+ +-+-+-+-+-+-+-+ */
  // create dedicated polling thread
  pthread_t poller;
  if ((ret = pthread_create(&poller, NULL, (void *(*)(void *))poll_connections,
                            (void *)&listener) ||
             pthread_detach(poller))) {
    handle_error_en(ret, "pthread_{create, detach}");
  }

  /* +-+-+-+-+-+-+-+ +-+-+-+-+ +-+-+-+-+-+ */
  /* |P|R|O|C|E|S|S| |U|S|E|R| |I|N|P|U|T| */
  /* +-+-+-+-+-+-+-+ +-+-+-+-+ +-+-+-+-+-+ */
  char msg[MAXBUFSIZ];
  memset(msg, 0, sizeof(msg));
  // loop until REPL receives 'q' or '<C-D>' to stop.
  while (fgets(msg, MAXBUFSIZ, stdin)) {
    process_input(msg, MAXBUFSIZ);
    if (check_stopped(&server_control))
      break;
  }

  // cleanup label XD honestly not a bad idea I think; I'd rather not wrap in a
  // bunch of scoped stuff
  // cleanup:

  /* +-+-+-+-+-+-+-+ */
  /* |C|L|E|A|N|U|P| */
  /* +-+-+-+-+-+-+-+ */
  // set to stopped, in case someone typed <C-D>
  pthread_mutex_lock(&server_control.server_mtx);
  server_control.stopped = 1;
  pthread_mutex_unlock(&server_control.server_mtx);

  printf("Exiting snowcast server...\n");

  // close listener socket
  close(listener);

  // wait for threads to finish
  wait_thread_pool(server_control.t_pool);

  // cancel polling thread, so it releases the client control mutex
  ret = pthread_cancel(poller);
  if (ret)
    handle_error_en(ret, "main: pthread_cancel");

  printf("Closed listener socket and shut down poller thread.\n");

  // destroy control structs
  destroy_client_control(&client_control);
  destroy_station_control(&station_control);
  destroy_server_control(&server_control);

  printf("Goodbye!\n");
  return 0;
}

/* ===============================================================================
 *                              HELPER FUNCTIONS
 * ===============================================================================
 */

// So this doesn't actually return -1 on failure, it just exits. This is fine
// since it's the first init call, but I may want to change how I report errors
// here in the future.
int init_server_control(server_control_t *server_control, size_t num_threads) {
  // initialize synchronization primitives
  int ret = pthread_mutex_init(&server_control->server_mtx, NULL) ||
            pthread_cond_init(&server_control->server_cond, NULL);
  if (ret)
    handle_error_en(ret, "init_server_control: pthread_{mutex,cond}_init");

  // attempt to create thread pool
  server_control->t_pool = init_thread_pool(num_threads);
  if (server_control->t_pool == NULL) {
    // cleanup on failure
    ret = pthread_mutex_destroy(&server_control->server_mtx) ||
          pthread_cond_destroy(&server_control->server_cond);
    if (ret)
      handle_error_en(ret, "init_server_control: pthread_{mutex,cond}_destroy");
  }
  server_control->stopped = 0;

  return 0;
}

// TODO: FIX UP, I NEED TO HANDLE CLEANUP
void destroy_server_control(server_control_t *server_control) {
  // destroy thread pool
  destroy_thread_pool(server_control->t_pool);

  // destroy synchronization primitives
  int ret = pthread_mutex_destroy(&server_control->server_mtx) ||
            pthread_cond_destroy(&server_control->server_cond);
  if (ret)
    handle_error_en(ret, "init_server_control: pthread_{mutex,cond}_destroy");
}

int init_station_control(station_control_t *station_control,
                         size_t num_stations, char *songs[]) {
  // attempt to malloc enough space for the stations
  station_control->stations = malloc(num_stations * sizeof(station_t *));
  if (station_control->stations == NULL) {
    fprintf(stderr,
            "[init_station_control] Could not malloc stations array.\n");
    return -1;
  }

  station_control->num_stations = num_stations;
  // attempt to init every station
  for (size_t i = 0; i < num_stations; i++) {
    station_control->stations[i] = init_station(i, songs[i]);
    if (station_control->stations[i] == NULL) {
      // cleanup previously initialized stations
      for (int j = 0; j < i; j++)
        destroy_station(station_control->stations[i]);
      free(station_control->stations);
      return -1;
    }
  }

  // initialize mutex
  int ret = pthread_mutex_init(&station_control->station_mtx, NULL);
  if (ret) {
    // TODO: print better output
    fprintf(stderr, "[init_station_control] Failed to initialize mutex.\n");
    // if failure, cleanup previously initialized stations
    for (size_t i = 0; i < num_stations; i++)
      destroy_station(station_control->stations[i]);
    free(station_control->stations);
    return -1;
  }

  return 0;
}

void destroy_station_control(station_control_t *station_control) {
  // lock access to prevent others from editing stations while destroying
  pthread_mutex_lock(&station_control->station_mtx);
  // cleanup all stations
  for (size_t i = 0; i < station_control->num_stations; i++)
    destroy_station(station_control->stations[i]);
  // free stations array
  free(station_control->stations);

  // unlock and destroy mutex
  pthread_mutex_unlock(&station_control->station_mtx);

  printf("Stopped stations. ");

  int ret = pthread_mutex_destroy(&station_control->station_mtx);
  if (ret) {
    // TODO: print better output
    fprintf(stderr, "[destroy_station_control] Failed to destroy mutex.\n");
  }
}

int init_client_control(client_control_t *client_control, int listener) {
  int ret = init_client_vector(&client_control->client_vec, INIT_MAX_CLIENTS,
                               listener);
  if (ret)
    return -1;

  client_control->num_pending = 0;
  if ((ret = pthread_mutex_init(&client_control->clients_mtx, NULL) ||
             pthread_cond_init(&client_control->pending_cond, NULL))) {
    // TODO: print better output
    fprintf(stderr, "[init_station_control] Failed to init cv/mutex.\n");
    destroy_client_vector(&client_control->client_vec);
    return -1;
  }

  return 0;
}

void destroy_client_control(client_control_t *client_control) {
  // first, try lock then unlock to ensure that mutex is properly cleaned up
  pthread_mutex_lock(&client_control->clients_mtx);
  // also synchronizes client cleanup
  destroy_client_vector(&client_control->client_vec);
  pthread_mutex_unlock(&client_control->clients_mtx);

  printf("Destroyed client information.\n");

  // now, destroy mutex and cv
  int ret = pthread_mutex_destroy(&client_control->clients_mtx);
  if (ret) {
    handle_error_en(ret, "destroy_client_control: pthread_mutex_destroy");
  }

  ret = pthread_cond_destroy(&client_control->pending_cond);
  if (ret)
    handle_error_en(ret, "destroy_client_control: pthread_cond_destroy");
}

void process_input(char *msg, size_t size) {
  // if error, EOF, or 'q', mark server as stopped
  if (msg[0] == 'q') {
    pthread_mutex_lock(&server_control.server_mtx);
    server_control.stopped = 1;
    pthread_mutex_unlock(&server_control.server_mtx);
    // otherwise, print information
  } else if (msg[0] == 'p') {
    // prevent changes to stations while we print
    // [TODO: maybe change to rwlock?]
    // TODO: this isn't necessary until I implement adding servers. Blegh!
    pthread_mutex_lock(&station_control.station_mtx);

    station_t *station;
    // for every station, print the current song and connected clients.
    for (size_t i = 0; i < station_control.num_stations; i++) {
      station = station_control.stations[i];
      printf("[Station %d: \"%s\"] Listening:\n", station->station_number,
             station->song_name);
      // iterate through connected clients
      client_connection_t *conn;
      sync_list_iterate_begin(&station->client_list, conn, client_connection_t,
                              link) {
        // clear buffer
        memset(msg, 0, size);
        // get and print client info
        get_address(msg, conn->udp_addr);
        printf("\t - %s\n", msg);
      }
      sync_list_iterate_end(&station->client_list);
    }

    // allow changes again
    pthread_mutex_unlock(&station_control.station_mtx);
  } else {
    fprintf(
        stderr,
        "Invalid input! Usage: \n"
        "\t'p': Print all stations, their current songs, and who's connected.\n"
        "\t'q': Terminate the server.\n");
  }
}

int check_stopped(server_control_t *server_control) {
  pthread_mutex_lock(&server_control->server_mtx);
  int stopped = server_control->stopped;
  pthread_mutex_unlock(&server_control->server_mtx);
  return stopped;
}

void atomic_incr(size_t *val, pthread_mutex_t *mtx) {
  pthread_mutex_lock(mtx);
  *val += 1;
  pthread_mutex_unlock(mtx);
}

void swap_stations(station_control_t *sc, client_connection_t *conn,
                   int new_station) {
  int old_station = conn->current_station;
  // if not currently in a station, join that one
  if (old_station == -1) {
    conn->current_station = new_station;
    accept_connection(sc->stations[new_station], conn);
  } else if (old_station != new_station) {
    int lower_station = old_station < new_station ? old_station : new_station;
    int higher_station = old_station < new_station ? new_station : old_station;
    // otherwise, establish absolute order: lock lower station first
    pthread_mutex_lock(&sc->stations[lower_station]->client_list.mtx);
    pthread_mutex_lock(&sc->stations[higher_station]->client_list.mtx);

    conn->current_station = new_station;
    // remove from old station, then add to new
    remove_connection(sc->stations[old_station], conn);
    accept_connection(sc->stations[new_station], conn);

    // unlock
    pthread_mutex_unlock(&sc->stations[higher_station]->client_list.mtx);
    pthread_mutex_unlock(&sc->stations[lower_station]->client_list.mtx);
  } else {
    // if identical, do nothing
    return;
  }
}

/* ===============================================================================
 *                              THREAD FUNCTIONS
 * ===============================================================================
 */

void process_connection(void *arg) {
  int listener = *(int *)arg, client_fd;

  // store connection information
  char address[MAXADDRLEN];
  struct sockaddr_storage from_addr;
  socklen_t addr_len = sizeof(from_addr);

  // accept client; this shouldn't block, since it's only called upon return
  // from poll.
  client_fd = accept(listener, (struct sockaddr *)&from_addr, &addr_len);
  if (client_fd == -1) {
    // if errors, exit function prematurely
    perror("[process_connection] accept");
    return;
  }

  get_address(address, (struct sockaddr *)&from_addr);
  printf("Received client connection from %s.\n", address);
  printf("Awaiting a Hello... ");

  // wait for a response; this times out after 100ms, so if client still doesn't
  // send HELLO, they get disconnected.
  uint8_t type;
  int res;
  void *msg = recv_command_msg(client_fd, &type, &res);
  // if NULL, an error occurred while recving, close the connection
  if (msg == NULL) {
    fprintf(stderr,
            "[process_connection] Failed to receive message from client %s. "
            "Closing connection...\n",
            address);
    close(client_fd);
    return;
  }
  // if reply type is not MESSAGE_HELLO, close the connection
  if (type != MESSAGE_HELLO) {
    fprintf(stderr,
            "Client %s sent incorrect initial message. Expected: %s\tGot: %s\n",
            address, "MESSAGE_HELLO",
            type > MESSAGE_SET_STATION ? "NEITHER" : "MESSAGE_SET_STATION");
    close(client_fd);
    return;
  }
  // get UDP port from message, then free it (we don't need anymore)
  uint16_t udp_port = ((hello_t *)msg)->udp_port;
  free(msg);

  printf("Received Hello!\nSending Welcome... ");

  // get num stations
  pthread_mutex_lock(&station_control.station_mtx);
  size_t num_stations = station_control.num_stations;
  pthread_mutex_unlock(&station_control.station_mtx);

  // synchronize access to client connections
  pthread_mutex_lock(&client_control.clients_mtx);
  // wrap in do {...} while(0) to allow breaking
  do {
    // attempt to add client
    int index;
    if ((index = add_client(&client_control.client_vec, client_fd, udp_port,
                            (struct sockaddr *)&from_addr, addr_len)) == -1) {
      printf("Closing client fd: %d\n", client_fd);
      close(client_fd);
      break;
    }
    // send "Welcome" reply message; if fails, close stuff
    if (send_reply_msg(client_fd, REPLY_WELCOME, num_stations, NULL)) {
      fprintf(stderr, "Failed to send Welcome. Closing connection.\n");
      remove_client(&client_control.client_vec, index);
      break;
    }
  } while (0);

  printf("Done!\n");

  // unlock; if we're the last pending operation, signal to cv
  if (--client_control.num_pending == 0)
    pthread_cond_signal(&client_control.pending_cond);
  pthread_mutex_unlock(&client_control.clients_mtx);
}

void pthread_unlock_cleanup_handler(void *arg) {
  pthread_mutex_unlock((pthread_mutex_t *)arg);
}

void *poll_connections(void *arg) {
  int listener = *(int *)arg;
  int num_events;
  size_t num_fds;
  struct pollfd *pfds;
  // repeat until stopped
  while (!check_stopped(&server_control)) {
    // first, check if any client operations are still pending
    pthread_mutex_lock(&client_control.clients_mtx);
    pthread_cleanup_push(pthread_unlock_cleanup_handler,
                         &client_control.clients_mtx);
    while (client_control.num_pending > 0) {
      printf("Waiting...\n");
      pthread_cond_wait(&client_control.pending_cond,
                        &client_control.clients_mtx);
    }

    // check to resize
    /* resize_client_vector(&client_control.client_vec, -1); */

    // once it's safe to attempt, poll indefinitely until a request/connection
    //
    // [Aside: how is safety guaranteed? In this implementation, this function
    // (i.e. the poller) is the only one capable of appending tasks that can
    // modify the client control vector; thus, as long as no tasks added by this
    // function are waiting, we can synchronously poll.]
    num_fds = client_control.client_vec.size + 1;
    pfds = client_control.client_vec.pfds;

    num_events = 0;
    num_events = poll(pfds, num_fds, -1);

    printf("Finished polling.\n");

    // once we're done polling, we can unlock the mutex
    pthread_cleanup_pop(1);

    // if no event (somehow) or an error occurred, go again
    if (num_events <= 0) {
      if (num_events < 0)
        perror("poll_connections: poll");
      continue;
    }

    // if listener has something, handle its connection
    if (pfds[0].revents & POLLIN) {
      atomic_incr(&client_control.num_pending, &client_control.clients_mtx);
      process_connection(&listener);
    }

    // loop through all client connections
    // TODO: handle synchronously removing connections
    for (size_t i = 1; i < num_fds; i++) {
      printf("num_fds: %zu\tindex: %zu\tcurrent socket: %d\n", num_fds, i,
             pfds[i].fd);
      // if there's a request, spawn a worker thread to deal with it
      if (pfds[i].revents & POLLIN) {
        // indicate that we're going to change the client list
        atomic_incr(&client_control.num_pending, &client_control.clients_mtx);
        handle_request_t *args = malloc(sizeof(handle_request_t));
        args->sockfd = pfds[i].fd;
        args->index = i - 1;
        add_job(server_control.t_pool, handle_request, (void *)args);
      }
    }
  }

  return NULL;
}

void handle_request(void *arg) {
  // get arguments
  handle_request_t *args = (handle_request_t *)arg;
  int sockfd = args->sockfd, index = args->index, res;
  uint8_t type;

  printf("Entering handle_request from socket %d...\n", sockfd);
  // receive message from client
  void *msg = recv_command_msg(sockfd, &type, &res);
  // if failed to receive message, server closes connection
  if (res != 0) {
    fprintf(stderr, "[handle_request] See above. result: %d\n", res);
    // only close if it was client disconnecting
    if (res == 1) {
      pthread_mutex_lock(&client_control.clients_mtx);
      remove_client(&client_control.client_vec, index);
      pthread_mutex_unlock(&client_control.clients_mtx);
    }
  } else {
    // store message
    char buf[MAXBUFSIZ];
    memset(buf, 0, sizeof(buf));
    if (type == MESSAGE_SET_STATION) {
      uint16_t new_station = ((set_station_t *)buf)->station_number;
      // swap stations
      pthread_mutex_lock(&client_control.clients_mtx);
      swap_stations(&station_control, &client_control.client_vec.conns[index],
                    new_station);
      pthread_mutex_unlock(&client_control.clients_mtx);
      // synchronize access
      pthread_mutex_lock(&station_control.station_mtx);
      // get station's song
      char song_name[MAXSONGLEN];
      memset(song_name, 0, sizeof(song_name));
      strcpy(song_name, station_control.stations[new_station]->song_name);
      // no longer need synchronization
      pthread_mutex_unlock(&station_control.station_mtx);

      // send response to client
      sprintf(buf, "Switched to Station %d. Now playing: [\"%s\"]", new_station,
              song_name);
      printf("strlen(buf): %lu\n", strlen(buf));
      if (send_reply_msg(sockfd, REPLY_ANNOUNCE, strlen(buf), buf) == -1) {
        fprintf(stderr, "[handle_request] See above error messages.\n");
      }
    } else {
      // invalid command; indicate as such
      sprintf(buf, "Invalid command: Got %d, must be within [%s].\n", type,
              "MESSAGE_SET_STATION");
      send_reply_msg(sockfd, REPLY_INVALID, strlen(buf), buf);
    }

    printf("Sent reply message!\n");

    // free message when done
    free(msg);
  }

  // decrement and potentially signal to cv
  pthread_mutex_lock(&client_control.clients_mtx);
  if (--client_control.num_pending == 0)
    pthread_cond_signal(&client_control.pending_cond);
  pthread_mutex_unlock(&client_control.clients_mtx);
}
