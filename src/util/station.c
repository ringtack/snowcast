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

  sync_list_init(&(station->client_list));
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
  // initialize buffer
  memset(station->buf, 0, sizeof(station->buf));

  // start running streaming thread
  int ret;
  if ((ret = pthread_create(&station->streamer, NULL,
                            (void *(*)(void *))stream_music_loop, station)) ||
      pthread_detach(station->streamer)) {
    // clean up allocations
    fclose(song_file);
    free(station->song_name);
    free(station);
    handle_error_en(ret, "init_station: pthread_{create, detach}");
  }

  return station;
}

void destroy_station(station_t *station) {
  assert(station != NULL);

  // don't need to destroy every client; client_control handles that
  // this is just a mutex destroy; check if valid
  if (sync_list_destroy(&(station->client_list)))
    fprintf(stderr, "failed to destroy station %d's mutex.\n",
            station->station_number);

  // cancel thread [TODO: implement cleanup handler for cancels]; do this before
  // closing, to prevent use after free/close
  int ret = pthread_cancel(station->streamer);
  if (ret)
    handle_error_en(ret, "destroy_station: pthread_cancel");

  // free information that was allocated by making a string duplicate
  free(station->song_name);
  if (fclose(station->song_file) != 0)
    perror("destroy_station: fclose");

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
      printf("nbytes: %d\tbytesleft: %d\tret: %d\n", nbytes, bytesleft, ret);
      fprintf(stderr, "[Station %d] Failed to read from song %s.\n",
              station->station_number, station->song_name);
      return -1;
      // otherwise, we've reached the end of a file, but need to read more; so,
      // restart to beginning of song
    } else if (nbytes < total) {
      // TODO: send to every connected client
      /* printf("[Station %d] Finished song %s! Repeating...\n", */
      /* station->station_number, station->song_name); */
      if (fseek(station->song_file, 0, SEEK_SET) == -1) {
        perror("[read_chunk] fseek");
        return -1;
      }
    }
  }
  return 0;
}

int send_to_connections(station_t *station) {
  assert(station != NULL);

  int ret = 0;
  // concurrently iterate through each client connection
  client_connection_t *it;
  sync_list_iterate_begin(&station->client_list, it, client_connection_t,
                          link) {
    // TODO: remove sendtoall
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
void *stream_music_loop(void *arg) {
  station_t *station = (station_t *)arg;

  // record start/end of sending
  int ret;
  struct timeval tv_start, tv_end;
  suseconds_t elapsed, wait;

  // until something stops us, read from song file, then send to every client
  // note that each loop reads in 1/16 of 16 KiB, so we should try to finish 16
  // loops every second
  while (0) {
    // note time of the start of operations
    ret = gettimeofday(&tv_start, NULL);
    if (ret == -1) {
      perror("[stream_music_loop] gettimeofday");
      continue;
    }

    // read from song file
    if (read_chunk(station) == -1) // an error occurred, so quit
      break;

    // send to connections
    if (send_to_connections(station)) {
      // quit on error
      fprintf(stderr, "[stream_music_loop] Refer to error messages above.\n");
      break;
    }

    // note time of the end of operations
    ret = gettimeofday(&tv_end, NULL);
    elapsed = tv_end.tv_usec - tv_start.tv_usec;
    // 1 * 10^6 / 16 = 62500 microseconds initially, then subtract time elapsed
    // between start and end of operations
    wait = WAIT_TIME - elapsed;

    // only sleep if elapsed time is less than initial wait time
    if (wait > 0 && usleep(wait)) {
      perror("[stream_music_loop] usleep");
    }
  }

  return NULL;
}
