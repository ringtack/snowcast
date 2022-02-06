#ifndef __SNOWCAST_SERVER__
#define __SNOWCAST_SERVER__

#include "./util/protocol.h"
#include "./util/station.h"
#include "./util/thread_pool.h"

#define INIT_MAX_CLIENTS 5
#define INIT_NUM_THREADS 16
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
 * A server_control_t is responsible for server operations and appropriate
 * cleanup.
 *  - A thread pool will manage work requests from client connections.
 *  - server_mtx synchronizes access to the server. Only necessary for stopped.
 *  - server_cond allows the server to wait until cleanup is done.
 *  - stopped indicates when the server is stopped; 0 -> running, 1 -> stopped.
 */
typedef struct {
  thread_pool_t *t_pool;      // thread pool for polling work!
  pthread_mutex_t server_mtx; // synchronize access to server
  pthread_cond_t server_cond; // condition variable for cleanup
  uint8_t stopped;            // flag for server condition
} server_control_t;

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
  station_t **stations;        // available stations
  size_t num_stations;         // keeps track of the number of stations
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
 * Initializes a server control struct with the specified number of threads in
 * the thread pool.
 *
 * Inputs:
 * - server_control_t *server_control: the server control struct to initialize
 * - size_t num_threads: the number of threads in the thread pool
 *
 * Returns:
 * - 0 on success, -1 on failure
 */
int init_server_control(server_control_t *server_control, size_t num_threads);

/**
 * Cleans up a server control struct.
 *
 * Inputs:
 * - server_control_t *server_control: the server control struct to clean up
 */
void destroy_server_control(server_control_t *server_control);

/**
 * Initializes a station control struct with the specified number of stations
 * and a string array of song names.
 *
 * Inputs:
 * - station_control_t *station_control: the station control struct to
 * initialize
 * - size_t num_stations: the number of stations
 * - char *songs[]: the songs corresponding to each station
 *
 * Returns:
 * - 0 on success, -1 on failure
 */
int init_station_control(station_control_t *station_control,
                         size_t num_stations, char *songs[]);

/**
 * Cleans up a station control struct.
 *
 * Inputs:
 * - station_control_t *station_control: the station control struct to clean up
 */
void destroy_station_control(station_control_t *station_control);

/**
 * Initializes a client control struct.
 *
 * Inputs:
 * - client_control_t *client_control: the client control struct to initialize.
 *
 * Returns:
 * - 0 on success, -1 on failure
 */
int init_client_control(client_control_t *client_control);

/**
 * Cleans up a client control struct.
 *
 * Inputs:
 * - client_control_t *client_control: the client control struct to clean up
 */
void destroy_client_control(client_control_t *client_control);

#endif
