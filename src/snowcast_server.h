#ifndef __SNOWCAST_SERVER__
#define __SNOWCAST_SERVER__

#include "./util/protocol.h"
#include "./util/station.h"
#include "./util/thread_pool.h"

/*
 * Central Snowcast Server to receive connections and broadcast songs. The main
 * thread will be responsible for handling REPL commands, and thread cleanup if
 * necessary.
 *
 * A snowcast_server_t represents the server's operations.
 *  - Currently, a fixed number of stations are supported, but can be easily
 * expanded if necessary.
 *  - A thread manages the server REPL, which updates `stopped` upon request.
 *  - A thread pool will manage work requests from client connections.
 *  - server_mtx synchronizes access to the server.
 *
 */

typedef struct {
  pthread_mutex_t server_mtx; // synchronize access to server
  pthread_cond_t server_cond; // condition variable for cleanup
  thread_pool_t *t_pool;      // thread pool for polling work!
  uint8_t stopped;            // flag for server condition
} snowcast_server_t;

/**
 * Structure to control and modify access to stations.
 * - Stations will be stored in a dynamically sized array; although a fixed
 * number of stations are supported, new stations can be easily added if
 * necessary.
 *   - TODO: implement add_station, remove_station
 * - Each station has an associated mutex; this synchronizes access to the
 * stations list, allowing for the server to swap two clients in a thread-safe
 * manner.
 *   - Note that each station's clients are already stored in a synchronized
 *   doubly-linked list; why do we need another mutex?
 *      - TODO: good question do I actually need two lol
 */
typedef struct {
  station_t *stations;         // available stations
  uint16_t num_stations;       // keep track of number of stations
  pthread_mutex_t station_mtx; // mutex for station control access
} station_control_t;

/**
 * Structure to control and modify access to client connections.
 *  - clients_mtx synchronizes access to the structure.
 *
 */
typedef struct {
  client_conn_vector_t clients; // vector of currently connected clients
  struct pollfd *pfds;          // vector of client file descriptors to poll
  uint16_t num_clients;         // keep track of number of clients
  uint16_t max_clients;         // record current max size of the array
  pthread_mutex_t clients_mtx;  // synchronize access to client control
} client_control_t;

// TODO: INIT_SNOWCAST_SERVER, DESTROY_SNOWCAST_SERVER
// TODO: INIT_CLIENT_CONTROL, DESTROY_CLIENT_CONTROL

/**
 * Testing purposes!
 */
void test(char *argv[]);

#endif
