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

  printf("Usage: \n"
         "\t'p <file>': Print all stations, their current songs, and who's "
         "connected. Can optionally supply a file for output location.\n"
         "\t'q': Terminate the server.\n");

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
  lock_server_control(&server_control);
  server_control.stopped = 1;
  unlock_server_control(&server_control);

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
  destroy_station_control(&station_control);
  destroy_client_control(&client_control);
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
  lock_station_control(station_control);
  // cleanup all stations
  for (size_t i = 0; i < station_control->num_stations; i++)
    destroy_station(station_control->stations[i]);
  // free stations array
  free(station_control->stations);

  // unlock and destroy mutex
  unlock_station_control(station_control);

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
  lock_client_control(client_control);
  // also synchronizes client cleanup
  destroy_client_vector(&client_control->client_vec);
  unlock_client_control(client_control);

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

void lock_server_control(server_control_t *server_control) {
  pthread_mutex_lock(&server_control->server_mtx);
}

void unlock_server_control(server_control_t *server_control) {
  pthread_mutex_unlock(&server_control->server_mtx);
}

void lock_station_control(station_control_t *station_control) {
  pthread_mutex_lock(&station_control->station_mtx);
}

void unlock_station_control(station_control_t *station_control) {
  pthread_mutex_unlock(&station_control->station_mtx);
}

void lock_client_control(client_control_t *client_control) {
  pthread_mutex_lock(&client_control->clients_mtx);
}

void unlock_client_control(client_control_t *client_control) {
  pthread_mutex_unlock(&client_control->clients_mtx);
}

void process_input(char *msg, size_t size) {
  // if error, EOF, or 'q', mark server as stopped
  if (msg[0] == 'q') {
    lock_server_control(&server_control);
    server_control.stopped = 1;
    unlock_server_control(&server_control);
    // otherwise, print information
  } else if (msg[0] == 'p') {
    // prevent changes to stations while we print
    // [TODO: maybe change to rwlock?]
    // TODO: this isn't necessary until I implement adding servers. Blegh!
    lock_station_control(&station_control);
    station_t *station;
    char name[MAXBUFSIZ];
    // get file name, if it exists
    FILE *out =
        sscanf(&msg[1], "%255s", name) == 1 ? fopen(name, "w+") : stdout;
    if (out == NULL) {
      // TODO: do some error checking
      ;
    }
    // for every station, print the current song and connected clients.
    for (size_t i = 0; i < station_control.num_stations; i++) {
      station = station_control.stations[i];
      fprintf(out, "%d,%s", station->station_number, station->song_name);
      client_connection_t *conn;
      sync_list_iterate_begin(&station->client_list, conn, client_connection_t,
                              link) {
        // clear buffer
        memset(msg, 0, size);
        // get and print client info
        get_address(msg, (struct sockaddr *)&conn->udp_addr);
        fprintf(out, ",%s", msg);
      }
      sync_list_iterate_end(&station->client_list);
      fprintf(out, "\n");
    }

    // close file if we don't need anymore
    if (out != stdout)
      fclose(out);

    // allow changes again
    unlock_station_control(&station_control);
  }
}

int check_stopped(server_control_t *server_control) {
  lock_server_control(server_control);
  int stopped = server_control->stopped;
  unlock_server_control(server_control);
  return stopped;
}

size_t get_num_stations(station_control_t *station_control) {
  lock_station_control(station_control);
  size_t num_stations = station_control->num_stations;
  unlock_station_control(station_control);
  return num_stations;
}

void atomic_incr(size_t *val, pthread_mutex_t *mtx) {
  pthread_mutex_lock(mtx);
  *val += 1;
  pthread_mutex_unlock(mtx);
}

int swap_stations(station_control_t *sc, client_connection_t *conn,
                  int new_station, int num_stations) {
  // verify that station is valid
  if (new_station >= num_stations || new_station < 0) {
    /* fprintf(stderr, */
    /* "[swap_stations] Client desired invalid station (wanted station " */
    /* "%d, but have %d stations).\n", */
    /* new_station, num_stations); */
    return -1;
  }

  int old_station = conn->current_station;
  // if not currently in a station, join that one
  if (old_station == -1) {
    conn->current_station = new_station;
    // synchronously add to list
    lock_station_clients(sc->stations[new_station]);
    accept_connection(sc->stations[new_station], conn);
    unlock_station_clients(sc->stations[new_station]);
  } else if (old_station != new_station) {
    int lower_station = old_station < new_station ? old_station : new_station;
    int higher_station = old_station < new_station ? new_station : old_station;
    // otherwise, establish absolute order: lock lower station first
    lock_station_clients(sc->stations[lower_station]);
    lock_station_clients(sc->stations[higher_station]);

    conn->current_station = new_station;
    // remove from old station, then add to new
    remove_connection(conn);
    accept_connection(sc->stations[new_station], conn);

    // unlock
    unlock_station_clients(sc->stations[higher_station]);
    unlock_station_clients(sc->stations[lower_station]);
  }
  // if identical, do nothing
  return 0;
}

void remove_client_from_server(client_control_t *cc, station_control_t *sc,
                               int sockfd) {
  // get which connection it is
  lock_client_control(cc);
  int index = get_client_index(&cc->client_vec, sockfd);
  if (index == -1) {
    unlock_client_control(cc);
    return;
  }
  client_connection_t *conn = get_client(&cc->client_vec, index);
  // in this case, we've already swapped it with a different value, so we can
  // just ignore
  if (conn == NULL) {
    unlock_client_control(cc);
    return;
  }
  int which_station = conn->current_station;

  // only remove from station if client is actually connected
  if (which_station >= 0) {
    // lock station
    lock_station_clients(sc->stations[which_station]);
    // remove from station
    remove_connection(conn);
    // successfully cleaned up from station
    unlock_station_clients(sc->stations[which_station]);
  }

  // remove client from client vector
  remove_client(&cc->client_vec, index);

  // done with client control
  unlock_client_control(cc);
}

/* ===============================================================================
 *                              THREAD FUNCTIONS
 * ===============================================================================
 */

void process_connection(void *arg) {
  int listener = *(int *)arg;

  // store connection information
  char address[MAXADDRLEN];
  struct sockaddr_storage from_addr;
  socklen_t addr_len = sizeof(from_addr);

  // wrap in do {...} while(0) to allow breaking
  // attempt to add client
  do {
    // accept client; this shouldn't block, since it's only called upon return
    // from poll.
    int client_fd = accept(listener, (struct sockaddr *)&from_addr, &addr_len);
    if (client_fd == -1) {
      // if errors, exit function prematurely
      perror("process_connection: accept");
      break;
    }

    get_address(address, (struct sockaddr *)&from_addr);
    printf("[Client %d] New client connected from %s; Awaiting a Hello...\n",
           client_fd, address);

    // wait for a response; this times out after 100ms, so if client still
    // doesn't send HELLO, they get disconnected.
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
      break;
    }
    // if reply type is not MESSAGE_HELLO, close the connection
    if (type != MESSAGE_HELLO) {
      fprintf(
          stderr,
          "[Client %d] Sent incorrect initial message. Expected: %s\tGot: %s\n",
          client_fd, "MESSAGE_HELLO",
          type > MESSAGE_SET_STATION ? "INVALID TYPE" : "MESSAGE_SET_STATION");
      fprintf(stderr, "Closing connection [%d]...\n", client_fd);
      close(client_fd);
      break;
    }
    // get UDP port from message, then free it (we don't need anymore)
    uint16_t udp_port = ((hello_t *)msg)->udp_port;
    free(msg);

    printf("[Client %d] Received Hello! Sending Welcome...\n", client_fd);

    // get num stations
    size_t num_stations = get_num_stations(&station_control);

    // synchronize access to client connections
    lock_client_control(&client_control);
    int index = add_client(&client_control.client_vec, client_fd, udp_port,
                           (struct sockaddr *)&from_addr, addr_len);
    // on failure, close client connection and stop
    if (index == -1) {
      close(client_fd);
      unlock_client_control(&client_control);
      break;
    }
    // send "Welcome" reply message; if fails, close stuff
    if (send_reply_msg(client_fd, REPLY_WELCOME, num_stations, NULL)) {
      fprintf(stderr, "Failed to send Welcome. Closing connection.\n");
      remove_client(&client_control.client_vec, index);
      unlock_client_control(&client_control);
      break;
    }
    unlock_client_control(&client_control);
  } while (0);

  lock_client_control(&client_control);
  // unlock; if we're the last pending operation, signal to cv
  if (--client_control.num_pending == 0)
    pthread_cond_signal(&client_control.pending_cond);
  unlock_client_control(&client_control);
}

void pthread_unlock_cleanup_handler(void *arg) {
  printf("Polling thread cancelled. Unlocking mutex...\n");
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
    lock_client_control(&client_control);
    pthread_cleanup_push(pthread_unlock_cleanup_handler,
                         &client_control.clients_mtx);
    while (client_control.num_pending > 0) {
      pthread_cond_wait(&client_control.pending_cond,
                        &client_control.clients_mtx);
    }

    // check to resize
    resize_client_vector(&client_control.client_vec, -1);

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

    // once we're done polling, we can unlock the mutex
    // only execute cleanup on error, since it has a print statement
    pthread_cleanup_pop(0);
    unlock_client_control(&client_control);

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
    for (size_t i = 1; i < num_fds; i++) {
      // if there's a request, spawn a worker thread to deal with it
      if (client_control.client_vec.pfds[i].revents & POLLIN) {
        // indicate that we're going to change the client list
        atomic_incr(&client_control.num_pending, &client_control.clients_mtx);
        handle_request_t *args = malloc(sizeof(handle_request_t));
        args->sockfd = client_control.client_vec.pfds[i].fd;
        add_job(server_control.t_pool, handle_request, (void *)args);
      }
    }
  }

  return NULL;
}

void handle_request(void *arg) {
  // get arguments
  handle_request_t *args = (handle_request_t *)arg;
  int sockfd = args->sockfd, res;
  uint8_t type;

  // receive message from client
  void *msg = recv_command_msg(sockfd, &type, &res);

  // if failed to receive message, server closes connection
  if (res != 0) {
    // this case handles if msg == NULL! Only close if client disconnected.
    if (res == -1) {
      fprintf(stderr, "[Client %d] Invalid command type.\n", sockfd);
    }
    remove_client_from_server(&client_control, &station_control, sockfd);
  } else {
    // sanity check; recv_command_msg should only be NULL if res != 0
    assert(msg != NULL);

    // store message
    char buf[MAXBUFSIZ];
    memset(buf, 0, sizeof(buf));
    if (type == MESSAGE_SET_STATION) {
      // get num stations
      int num_stations = get_num_stations(&station_control);

      // swap stations
      uint16_t new_station = ((set_station_t *)msg)->station_number;
      lock_client_control(&client_control);
      int index = get_client_index(&client_control.client_vec, sockfd);
      assert(index != -1);
      if (index == -1) {
        unlock_client_control(&client_control);
        return;
      }
      res = swap_stations(&station_control,
                          get_client(&client_control.client_vec, index),
                          new_station, num_stations);
      unlock_client_control(&client_control);

      // if they had invalid set stations request, send invalid request reply
      if (res == -1) {
        sprintf(buf,
                "Requested station %d, but server only has stations [0, %d).",
                new_station, num_stations);
        send_reply_msg(sockfd, REPLY_INVALID, strlen(buf), buf);

        // print to server, then close connection
        fprintf(stderr, "[Client %d] %s\n", sockfd, buf);
        remove_client_from_server(&client_control, &station_control, sockfd);
      } else {
        // otherwise, announce to client that station switch was successful
        // synchronize access
        lock_station_control(&station_control);

        // get station's song
        char song_name[MAXSONGLEN];
        memset(song_name, 0, sizeof(song_name));
        strcpy(song_name, station_control.stations[new_station]->song_name);

        // no longer need synchronization
        unlock_station_control(&station_control);

        // send response to client
        sprintf(buf, "\"%s\" [switched to Station %d]", song_name, new_station);
        if (send_reply_msg(sockfd, REPLY_ANNOUNCE, strlen(buf), buf) == -1) {
          // on failure, remove client from connections
          fprintf(stderr, "[handle_request] See above error messages.\n");
          remove_client_from_server(&client_control, &station_control, sockfd);
        }

        printf("[Client %d] Switched to station %d.\n", sockfd, new_station);
      }
    } else {
      // invalid command; indicate as such
      sprintf(buf, "got command of type %d, but must be within [%s].", type,
              "MESSAGE_SET_STATION");
      send_reply_msg(sockfd, REPLY_INVALID, strlen(buf), buf);

      // remove client from server
      fprintf(stderr, "[Client %d] %s\n", sockfd, buf);
      remove_client_from_server(&client_control, &station_control, sockfd);
    }
    // free message when done
    free(msg);
  }

  // decrement and potentially signal to cv
  lock_client_control(&client_control);
  if (--client_control.num_pending == 0)
    pthread_cond_signal(&client_control.pending_cond);
  unlock_client_control(&client_control);
}
