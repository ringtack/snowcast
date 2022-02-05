#ifndef __SNOWCAST_SERVER__
#define __SNOWCAST_SERVER__

#include "./util/protocol.h"
#include "./util/station.h"

#define POLLFD(new_fd)                                                         \
  (struct pollfd) { .fd = new_fd, .events = POLLIN }

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
  station_t **stations;       // available stations
  uint16_t num_stations;      // keep track of number of stations
  pthread_mutex_t server_mtx; // synchronize access to server
  pthread_t server_repl;      // thread to run the server REPL
  uint8_t stopped; // flag for server condition: 0 -> running, 1 -> stopped
  // pthread_cond_t server_cond; // condition variable FOR EXTRA FUNCTIONALITY
  // thread_pool_t th_pool; // [TODO: implement a thread pool]
} snowcast_server_t;

/**
 * Structure to control and modify access to client connections.
 *  - Clients are stored in a dynamically sized array; access must be
 * synchronized with the client mutex
 *  - pfds stores struct pollfds to asynchronously wait for incoming requests,
 * then handle each.
 *  - clients_mtx synchronizes access to the structure.
 *
 *  NOTE: `clients` AND `pfds` MUST BE EXACTLY SYNCHRONIZED: INDEX `i` IN
 * `clients` MUST CORRESPOND TO THE APPROPRIATE SOCKET FD IN `pfds`.
 *
 */
typedef struct {
  client_connection_t *clients; // currently connected clients
  struct pollfd *pfds;          // client file descriptors to poll
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
