#ifndef __CLIENT_CONNECTION_H__
#define __CLIENT_CONNECTION_H__

#include "list.h"
#include "util.h"

// helper for easy definitions of pollfds from fds
#define POLLFD(new_fd)                                                         \
  (struct pollfd) { .fd = new_fd, .events = POLLIN }

/**
 * Struct representing a single client connection.
 * - Both tcp_addr and udp_addr are needed to send information to both the
 * listener and control endpoints. One could set the UDP address of tcp_addr,
 * send information, then reset the port, but that's kinda clunky ¯\_(ツ)_/¯
 *
 * For the struct sockaddrs, I chose to use malloc instead of having a static
 * struct; this way, I could support both IPv4 and IPv6. I have now learned
 * about `struct sockaddr_storage`; when/if I have the time, I'll migrate the
 * implementation.
 * - TODO: Migrate from dynamically allocated `struct sockaddrs` to statically
 * allocated `struct sockaddr_storage`s.
 */
typedef struct {
  list_link_t link;          // for the doubly linked lists
  int client_fd;             // TCP connection socket
  struct sockaddr *tcp_addr; // TCP address
  struct sockaddr *udp_addr; // UDP address
  socklen_t addr_len;        // length of address; only difference is port
} client_connection_t;

/**
 * Struct representing a vector of clients.
 *  - Client connections and pollfds are each stored in a dynamically sized
 * array; vector operations may be assumed for insertion/deletion from the
 * vector.
 *
 *  NOTE: `conns` AND `pfds` MUST BE EXACTLY SYNCHRONIZED: INDEX `i` IN
 * `conns` MUST CORRESPOND TO THE APPROPRIATE SOCKET FD IN `pfds`.
 *    - this separation is necessary to support the poll(2) system call;
 *    otherwise, I'd really like to encapsulate this into one connection :/
 */
typedef struct {
  client_connection_t *conns; // array of connections
  struct pollfd *pfds;        // array of struct pollfds
  size_t size;                // current size of a vector array
  size_t max;                 // current max size of a vector array
} client_vector_t;

/**
 * Initializes a client vector with the specified size.
 *
 * Inputs:
 * - client_vector_t *client_vec: the client vector to fill
 * - size_t max: the initial max size
 */
void init_client_vector(client_vector_t *client_vec, size_t max);

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
 * - client_vector_t *client_vec: pointer to a vector of client connections
 * - int client_fd: the client's socket file descriptor
 * - uint16_t udp_port: the UDP port of the client's listener
 * - struct sockaddr *sa: the client's socket address
 * - socklen_t sa_len: the length of the client's socket address
 *
 * Returns:
 * - 0 on success, -1 on failure
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
 * Resizes the client vector:
 *  - if positive, must have new_max >= size
 *  - if negative, shrinks if appropriate i.e. size < max / 2.
 *
 * Inputs:
 * - client_vector_t *client_vec: pointer to a vector of client connections
 * - int new_max: desired reallocation size
 */
void resize_client_vector(client_vector_t *client_vec, int new_max);

/**
 * Initializes a client connection given a client socket, UDP port, and sockaddr
 *
 * Inputs:
 * - client_connection_t *conn: the connection to initialize
 * - int client_fd: the client's socket file descriptor
 * - uint16_t udp_port: the UDP port of the client's listener
 * - struct sockaddr *sa: the client's socket address
 * - socklen_t sa_len: the length of the client's socket address
 *
 * Returns:
 * - 0 on success, -1 on failure
 */
int init_connection(client_connection_t *conn, int client_fd, uint16_t udp_port,
                    struct sockaddr *sa, socklen_t sa_len);

/**
 * Destroys a dynamically initialized connection, closing the client file
 * descriptor and freeing dynamically allocated data.
 *
 * Inputs:
 * - client_connection_t *conn: the connection to free
 */
void destroy_connection(client_connection_t *conn);

#endif

// lol jk i don't need this anymore
// keeping it for the nice banner tho
/* ===============================================================================
 *                           OLD HELPER FUNCTIONS
 * ===============================================================================
 * Once upon a time, these were useful; however, when making a dynamically sized
 * array of dynamically allocated client connections, I didn't really want to
 * work with a triple pointer situation. Thus, I encapsulated a client
 * connection with its corresponding pollfd into a vector. See above!
 */
