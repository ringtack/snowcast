#ifndef __SNOWCAST_SERVER__
#define __SNOWCAST_SERVER__

#include "./util/protocol.h"
#include "./util/station.h"
#include "./util/thread_pool.h"

/*
 *  ____                                   _     ____
 * / ___| _ __   _____      _____ __ _ ___| |_  / ___|  ___ _ ____   _____ _ __
 * \___ \| '_ \ / _ \ \ /\ / / __/ _` / __| __| \___ \ / _ \ '__\ \ / / _ \ '__|
 *  ___) | | | | (_) \ V  V / (_| (_| \__ \ |_   ___) |  __/ |   \ V /  __/ |
 * |____/|_| |_|\___/ \_/\_/ \___\__,_|___/\__| |____/ \___|_|    \_/ \___|_|
 *
 * Central Snowcast Server to receive connections and broadcast songs. The main
 * thread will be responsible for handling REPL commands, and thread cleanup if
 * necessary.
 */

/**
 * A snowcast_server_t represents the server's operations.
 *  - Currently, a fixed number of stations are supported, but can be easily
 *  expanded if necessary.
 *  - A thread pool will manage work requests from client connections.
 *  - server_mtx synchronizes access to the server.
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
 *   - TODO: implement station_vector_t
 *   - TODO: implement add_station, remove_station
 *
 * - Each station has an associated mutex; this synchronizes access to the
 * stations list, allowing for the server to swap two clients in a thread-safe
 * manner.
 *   - Note that each station's clients are already stored in a synchronized
 *   doubly-linked list; why do we need another mutex?
 *      - TODO: good question do I actually need two lol
 */
typedef struct {
  station_t **station_vec;     // available stations
  pthread_mutex_t station_mtx; // mutex for station control access
} station_control_t;

/**
 * Structure to control and modify access to client connections. Provides a
 * lightweight synchronization wrapper.
 */
typedef struct {
  client_vector_t client_vec;  // vector of currently connected clients
  pthread_mutex_t clients_mtx; // synchronize access to client control
} client_control_t;

/**
 * Testing purposes!
 */
void test(char *argv[]);

#endif
