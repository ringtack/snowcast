#ifndef __STATION_H__
#define __STATION_H__

/**
 * Data structure representing each individual station within the snowcast
 * server. Each station will be responsible for a list of its clients.
 */

#include "client_connection.h"
#include "sync_list.h"
#include "util.h"

#define CHUNK_SIZE 1024 // note 16384 / 16 = 1024
#define WAIT_TIME                                                              \
  62500 // microseconds! note 1000000 / 16 = 62500
        // TODO: shoul I do less (e.g. 60000) to account for loop time?

typedef struct {
  uint16_t station_number; // unique number for a station
  char *song_name;         // name of a song
  FILE *song_file;         // file to read the song
  char buf[CHUNK_SIZE];    // buffer to store processed song chunks
  sync_list_t client_list; // list to store clients connected to this station
} station_t;

// arg struct for multithreaded sendtoall calls
typedef struct {
  int sockfd;
  void *val;
  int len;
  struct sockaddr *sa;
  socklen_t sa_len;
} sta_args_t;

/**
 * Initializes a station given a station number and song name.
 *
 * Inputs:
 * - int station_number: the station number of this station
 * - char *song_name: the name of the song to play; must be a path!
 *
 * Returns:
 * - A dynamically allocated station on success, NULL on failure
 */
station_t *init_station(int station_number, char *song_name);

/**
 * Destroys a dynamically initialized station, closing the song file and freeing
 * dynamically allocated data (song name, struct itself).
 *
 * Inputs:
 * - station_t *station: station to free
 */
void destroy_station(station_t *station);

/**
 * Accepts a connection to the station.
 *
 * Inputs:
 * - station_t *station: the station of interest
 * - client_connection_t *conn: a dynamically allocated pointer to an incoming
 * connection
 */
void accept_connection(station_t *station, client_connection_t *conn);

/**
 * Removes a connection to the station.
 *
 * - station_t *station: the station of interest
 * - client_connection_t *conn: a dynamically allocated pointer to a
 * disconnectingconnection
 */
void remove_connection(station_t *station, client_connection_t *conn);

/**
 * Utility function to send all bytes of a value (UDP).
 *
 * Inputs (once we cast args):
 * - int sockfd: the connection socket
 * - void *val: the value to send
 * - int len: the size of the value
 * - struct sockaddr *sa: where we're sending
 * - socklen_t *sa_len: length of the sockaddr
 */
void sendtoall(void *arg);

/**
 * Reads a chunk from the station's song file, where CHUNK_SIZE = 16384 / 16 =
 * 1024B.
 *
 * Inputs:
 * - station_t *station: station to read
 *
 * Returns:
 * - 0 on success, non-zero on failure
 */
int read_chunk(station_t *station);

/**
 * Sends the buffer to each client, zeroing the buffer once done.
 *
 * Inputs:
 * - station_t *station: station with data to send
 *
 * Returns:
 * - 0 on success, non-zero on failure
 */
int send_to_connections(station_t *station);

/**
 * Threading utility function that infinitely sends chunks of data to all
 * clients, then waits a set amount of time to meet the 16KiB/s bandwidth
 * requirement.
 *
 * Inputs (once we cast args to station_t *):
 * - station_t *station: the station to run on a thread
 */
void stream_music_loop(void *arg);

#endif
