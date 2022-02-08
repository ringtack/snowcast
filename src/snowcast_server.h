#ifndef __SNOWCAST_SERVER__
#define __SNOWCAST_SERVER__

#include "util/client_vector.h"
#include "util/protocol.h"
#include "util/station.h"
#include "util/thread_pool.h"

#define INIT_MAX_CLIENTS 8
#define INIT_NUM_THREADS 8

#define MAXADDRLEN 64
#define MAXSONGLEN (MAXBUFSIZ / 2)

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
 *
 * - So I think `station_mtx` is necessary only for the number of stations
 * (`stations` already has synchronization primitives, until we implement adding
 * stations.)
 */
typedef struct {
  station_t **stations;        // available stations
  size_t num_stations;         // keeps track of the number of stations
  pthread_mutex_t station_mtx; // mutex for station control access
} station_control_t;

/**
 * Structure to control and modify access to client connections. Provides a
 * lightweight synchronization wrapper.
 * - Note that unless we want potentially messy signal handling, we need some
 * way of ensuring that the `pfds` in `client_vec` are not tampered with in the
 * middle of a `poll` call. To that end, we lock the client control struct until
 * poll returns. This, of course, may pose problems if we wish to modify the
 * client control struct in a timely manner; thus, we wait until all
 * modifications to the client control struct is done.
 */
typedef struct {
  client_vector_t client_vec;  // vector of currently connected clients
  size_t num_pending;          // record number of ops that change client_vec
  pthread_mutex_t clients_mtx; // synchronize access to client control
  pthread_cond_t pending_cond; // wait until no more pending
  // TODO: implement a signal handler
} client_control_t;

/* ===============================================================================
 *                              HELPER FUNCTIONS
 * ===============================================================================
 */

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
 * Initializes a client control struct with a listener socket.
 *
 * Inputs:
 * - client_control_t *client_control: the client control struct to initialize.
 * - int listener: the listener socket
 *
 * Returns:
 * - 0 on success, -1 on failure
 */
int init_client_control(client_control_t *client_control, int listener);

/**
 * Cleans up a client control struct.
 *
 * Inputs:
 * - client_control_t *client_control: the client control struct to clean up
 */
void destroy_client_control(client_control_t *client_control);

/**
 * Handles user input from stdin.
 * - On 'p', prints a list of stations, along with all clients connected to
 * them.
 * - On 'q', marks the server as stopped, which commences server cleanup and
 * termination.
 *
 * Inputs:
 * - char *msg: the user message
 * - size_t size: the max size of the buffer
 */
void process_input(char *msg, size_t size);

/**
 * Atomic check if stopped.
 *
 * Inputs:
 * - server_control_t *server_control: server to check
 *
 * Returns:
 * - the value of stopped
 */
int check_stopped(server_control_t *server_control);

/**
 * Atomically increments a size_t.
 *
 * Inputs:
 * - size_t *val: pointer to an size_t
 * - pthread_mutex_t *mtx: pointer to a mutex to synchronize
 */
void atomic_incr(size_t *val, pthread_mutex_t *mtx);

/**
 * Swaps the stations of a client; removes client from old station (if
 * applicable), and adds to new station.
 *
 * Inputs:
 * - station_control_t *sc: station control struct
 * - client_connection_t *conn: connection to swap
 * - int new_station: destination station
 *
 * Returns:
 * - 0 on success, -1 if invalid station
 */
int swap_stations(station_control_t *sc, client_connection_t *conn,
                  int new_station, int num_stations);

/*
===============================================================================
 *                        THREAD STRUCTURES/FUNCTIONS
 * ===============================================================================
 */

/*
 * NOTE: with all thread functions and structures, the "parent" function is
 * responsible for mallocing space for the args_t struct, and the thread
 * function is responsible for freeing that space.
 *
 * HOWEVER, if the function is meant for a thread pool, the thread pool will
 * take care of freeing the allocated memory.
 */

/**
 * Handles connections from TCP clients.
 *
 * Inputs:
 * - int listener: the listener socket
 */
void process_connection(void *arg);

/**
 * Polls client connections for requests. Note that whenever this appends a work
 * request to the thread pool, it must increment `client_control.num_pending`;
 * every work request is also responsible for decrementing it when done.
 *
 * Inputs:
 * - int listener: the listener socket
 */
void *poll_connections(void *arg);

typedef struct {
  int sockfd;
  int index;
} handle_request_t;

/**
 * Handles a request from a client. Currently, only SET_STATION is supported,
 * but ideally more could be in the future.
 *
 * Note that whenever this appends a work request to the thread pool, it must
 * increment `client_control.num_pending`; every work request is also
 * responsible for decrementing it when done.
 *
 * Inputs:
 * - int sockfd: the socket of the client connection
 * - int index: index of the client connection
 */
void handle_request(void *arg);

#endif
