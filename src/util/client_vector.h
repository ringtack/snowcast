#ifndef __CLIENT_VECTOR_H__
#define __CLIENT_VECTOR_H__

#include "client_connection.h"

// helper for easy definitions of pollfds from fds
#define POLLFD(new_fd)                                                         \
  (struct pollfd) { .fd = new_fd, .events = POLLIN }

/**
 * Struct representing a vector of clients.
 *  - Client connections and pollfds are each stored in a dynamically sized
 * array; vector operations may be assumed for insertion/deletion from the
 * vector.
 *
 *  NOTE: `conns` AND `pfds` MUST BE EXACTLY SYNCHRONIZED, **OFFSET BY 1**:
 * INDEX `i` IN `conns` MUST CORRESPOND TO THE APPROPRIATE SFD AT `pfds[i + 1]`.
 *    - this separation is necessary to support the poll(2) system call;
 *    otherwise, I'd really like to encapsulate this into one connection :/
 *
 * Unfortunately, there's no good way to synchronize access to the pollfds
 * struct while a thread is blocking `poll` on it; thus, we have to include
 * listening for incoming connections within this pollfd structure. This causes
 * an offset by 1 in the array.
 */
typedef struct {
  client_connection_t **conns; // array of connections
  struct pollfd *pfds;         // array of struct pollfds
  size_t size;                 // current size of a vector array
  size_t max;                  // current max size of a vector array
  int listener;                // listener socket
} client_vector_t;

/**
 * Initializes a client vector with the specified size and listener socket.
 *
 * Inputs:
 * - client_vector_t *client_vec: the client vector to fill
 * - size_t max: the initial max size
 * - int listener: the listener socket for connections
 *
 * Returns:
 * - 0 on success, -1 on failure
 */
int init_client_vector(client_vector_t *client_vec, size_t max, int listener);

/**
 * Destroys a client vector. Frees up all dynamically allocated portions inside!
 *
 * Inputs:
 * - client_vector_t *client_vec: client vector to free
 */
void destroy_client_vector(client_vector_t *client_vec);

/**
 * Adds a client connection to a vector of clients.
 *
 * Inputs:
 * - client_vector_t *client_vec: the client vector
 * - int client_fd: the TCP client's socket file descriptor
 * - uint16_t udp_port: the UDP's listening port
 * - struct sockaddr *sa: the TCP client's socket address
 * - socklen_t sa_len: the length of the socket addresses
 *
 * Returns:
 * - index where it was placed on success, -1 on failure
 */
int add_client(client_vector_t *client_vec, int client_fd, uint16_t udp_port,
               struct sockaddr *sa, socklen_t sa_len);

/**
 * Removes a client connection from a vector of client connections. DOES NOT
 * RESIZE TO ALLOW REMOVAL IN ITERATIONS! EXPLICITLY CALL resize_client_vector()
 * IF YOU WANT TO RESIZE.
 *
 * Inputs:
 * - client_vector_t *client_vec: pointer to a vector of client connections
 * - int index: index of client to delete
 */
void remove_client(client_vector_t *client_vec, int index);

/**
 * Gets the client at index i.
 *
 * Inputs:
 * - client_vector_t *client_vec: pointer to a vector of client connections
 * - int index: index of client to delete
 *
 * Returns:
 * - dynamically allocated connection, or NULL if invalid index
 */
client_connection_t *get_client(client_vector_t *client_vec, int index);

/**
 * Resizes the client vector:
 *  - if positive, must have new_max >= size
 *  - if negative, shrinks if appropriate i.e. size < max / 2.
 *
 * Inputs:
 * - client_vector_t *client_vec: pointer to a vector of client connections
 * - int new_max: desired reallocation size
 *
 * Returns:
 * - 0 on success, -1 on failure
 */
int resize_client_vector(client_vector_t *client_vec, int new_max);

#endif
