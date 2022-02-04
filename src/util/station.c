#include "station.h"

station_t *init_station(int station_number, char *song_name) {
  // attempt to open the file
  FILE *song_file = fopen(song_name, "r");
  if (song_file == NULL) {
    perror("init_station: fopen");
    return NULL;
  }

  // attempt to malloc space
  station_t *station = malloc(sizeof(station_t));
  if (station == NULL) {
    fprintf(stderr, "[init_station] Failed to malloc station %d.\n",
            station_number);
    // if failed, clean up previous allocations
    fclose(song_file);
    return NULL;
  }

  station->station_number = station_number;
  station->song_name = strdup(song_name);
  // if duplication of string name fails, return an error
  if (station->song_name == NULL) {
    fprintf(stderr, "init_station] Not enough memory for song name %s.\n",
            song_name);
    fclose(song_file);
    free(station);
    return NULL;
  }
  station->song_file = song_file;
  // initialize buffer and synchronized list
  memset(station->buf, 0, sizeof(station->buf));
  sync_list_init(&(station->client_list));

  return station;
}

void destroy_station(station_t *station) {
  assert(station != NULL);
  // destroy every client
  client_connection_t *client;
  sync_list_iterate_begin(&station->client_list, client, client_connection_t,
                          link) {
    destroy_connection(client);
  }
  // this is just a mutex destroy
  if (sync_list_destroy(&(station->client_list)))
    fprintf(stderr, "failed to destroy station %d's mutex.\n",
            station->station_number);
  // free information that was allocated by making a string duplicate
  free(station->song_name);
  if (fclose(station->song_file) != 0)
    perror("destroy_station: fclose");
  free(station);
}

void accept_connection(station_t *station, client_connection_t *conn) {
  sync_list_insert_tail(&station->client_list, &conn->link);
}

void remove_connection(station_t *station, client_connection_t *conn) {
  sync_list_remove(&station->client_list, &conn->link);
}

void sendtoall(void *arg) {
  sta_args_t *args = (sta_args_t *)arg;
  int total = 0;
  int bytesleft = args->len;
  int n;
  // while bytes sent < total bytes, attempt sending the rest
  while (total < args->len) {
    n = sendto(args->sockfd, args->val + total, bytesleft, 0, args->sa,
               args->sa_len);
    // if an error occurs while sending, return -1
    if (n == -1)
      fprintf(stderr, "[sendtoall] Error while sending bytes to %d.\n",
              args->sockfd);
    // otherwise, update counts
    total += n;
    bytesleft -= n;
  }
}

int read_chunk(station_t *station) {
  assert(station != NULL);

  // read until we've read a chunk
  int ret, nbytes, bytesleft, total;
  nbytes = 0;
  bytesleft = total = CHUNK_SIZE * sizeof(char);
  while (nbytes < total) {
    // read from file, and update numbytes and bytes left to read
    ret = fread(station->buf + nbytes, bytesleft, sizeof(char),
                station->song_file);
    nbytes += ret;
    bytesleft -= ret;
    // if 0, something went wrong
    if (ret == 0) {
      fprintf(stderr, "[Station %d] Failed to read from song %s.\n",
              station->station_number, station->song_name);
      return -1;
      // otherwise, we've reached the end of a file, but need to read more; so,
      // restart to beginning of song
    } else if (nbytes < total) {
      printf("[Station %d] Finished song %s! Repeating...\n",
             station->station_number, station->song_name);
      if (fseek(station->song_file, 0, SEEK_SET) == -1) {
        perror("read_chunk: fseek");
        return -1;
      }
    }
  }
  return ret;
}

int send_to_connections(station_t *station) {
  assert(station != NULL);

  int ret = 0;
  // lock client list
  pthread_mutex_lock(&station->client_list.mtx);
  // get size, and create that many threads
  size_t size = station->client_list.size;
  // must create array here, unless I want to malloc every thread (do...while
  // kills scope)
  pthread_t ths[size];
  // iterate through each client connection
  client_connection_t *it;
  int i;
  list_iterate_begin(&station->client_list.sync_list, it, client_connection_t,
                     link) {
    // make args struct
    sta_args_t args = {it->client_fd, station->buf, sizeof(station->buf),
                       it->addr, it->addr_len};
    // create thread
    if (pthread_create(&ths[i++], NULL, (void *(*)(void *))sendtoall, &args)) {
      ret = -1;
      fprintf(
          stderr,
          "[send_to_connections] Error creating thread for connection %d.\n",
          it->client_fd);
    }
  }
  list_iterate_end();

  // join all threads
  for (int i = 0; i < size; i++) {
    if (pthread_join(ths[i], NULL)) {
      ret = -1;
      fprintf(stderr, "[send_to_connections] Error while joining thread %d.\n",
              i);
    }
  }

  // unlock client list
  pthread_mutex_unlock(&station->client_list.mtx);

  // zero information once done
  memset(station->buf, 0, sizeof(station->buf));

  return ret;
}

void stream_music_loop(void *arg) {
  station_t *station = (station_t *)arg;

  // until something stops us, read from song file, then send to every client
  // note that each loop reads in 1/16 of 16 KiB, so we should try to finish 16
  // loops every second
  while (1) {
    // read from song file
    if (read_chunk(station)) // an error occurred, so quit
      return;

    // send to connections
    if (send_to_connections(station)) {
      // quit on error
      fprintf(stderr, "[stream_music_loop] Refer to error messages above.\n");
      return;
    }

    // 1 * 10^6 / 16 = 62500, so we wait for 62500 microseconds
    if (usleep(WAIT_TIME)) {
      perror("stream_music_loop: usleep");
      return;
    }
  }
}
