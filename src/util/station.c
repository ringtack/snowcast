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
    fprintf(stderr, "[init_station] Not enough memory for song name %s.\n",
            song_name);
    // clean up previous allocations
    fclose(song_file);
    free(station);
    return NULL;
  }
  station->song_file = song_file;
  // initialize buffer and synchronized list
  memset(station->buf, 0, sizeof(station->buf));
  sync_list_init(&(station->client_list));

  // start running streaming thread
  /* int ret = */
  /* pthread_create(&station->streamer, NULL, */
  /* (void *(*)(void *))stream_music_loop, (void *)&station); */
  /* if (!ret) { */
  /* // clean up allocations */
  /* fclose(song_file); */
  /* free(station->song_name); */
  /* free(station); */
  /* handle_error_en(ret, "init_station: pthread_create"); */
  /* } */

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
  sync_list_iterate_end(&station->client_list);

  // this is just a mutex destroy; check if valid
  if (sync_list_destroy(&(station->client_list)))
    fprintf(stderr, "failed to destroy station %d's mutex.\n",
            station->station_number);

  // free information that was allocated by making a string duplicate
  free(station->song_name);
  if (fclose(station->song_file) != 0)
    perror("destroy_station: fclose");

  // cancel then join thread [TODO: implement cleanup handler for cancels]
  int ret = pthread_cancel(station->streamer);
  if (!ret)
    handle_error_en(ret, "destroy_station: pthread_cancel");
  ret = pthread_join(station->streamer, NULL);
  if (!ret)
    handle_error_en(ret, "destroy_station: pthread_cancel");

  // free struct itself
  free(station);
}

void accept_connection(station_t *station, client_connection_t *conn) {
  sync_list_insert_tail(&station->client_list, &conn->link);
}

void remove_connection(station_t *station, client_connection_t *conn) {
  sync_list_remove(&station->client_list, &conn->link);
}

int read_chunk(station_t *station) {
  assert(station != NULL);

  // read until we've read a chunk
  int ret, nbytes, bytesleft, total;
  nbytes = 0;
  bytesleft = total = CHUNK_SIZE * sizeof(char);
  while (nbytes < total) {
    // read from file, and update numbytes and bytes left to read
    ret = fread(station->buf + nbytes, sizeof(char), bytesleft,
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
  // concurrently iterate through each client connection
  client_connection_t *it;
  sync_list_iterate_begin(&station->client_list, it, client_connection_t,
                          link) {
    if ((ret = sendtoall(it->client_fd, station->buf, sizeof(station->buf),
                         it->udp_addr, it->addr_len))) {
      fprintf(stderr,
              "[send_to_connections] Error sending data to connection %d.\n",
              it->client_fd);
    }
  }
  sync_list_iterate_end(&station->client_list);

  // zero information once done
  memset(station->buf, 0, sizeof(station->buf));

  return ret;
}

// TODO: set up thread cancel signal handler for each station
// TODO: potentially add to thread pool?
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
