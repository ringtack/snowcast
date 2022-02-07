#ifndef __CLIENT_CONNECTION_H__
#define __CLIENT_CONNECTION_H__

#include "list.h"
#include "util.h"

/**
 * Struct representing a single client connection.
 * - Both tcp_fd and udp_fd are needed to send information to both the listener
 * and control endpoints. tcp_addr and udp_addr are used to print information
 * about the IP/port address of each connection, if necessary.
 *
 * For the struct sockaddrs, I chose to use malloc instead of having a static
 * struct; this way, I could support both IPv4 and IPv6. I have now learned
 * about `struct sockaddr_storage`; when/if I have the time, I'll migrate the
 * implementation.
 * - TODO: Migrate from dynamically allocated `struct sockaddrs` to statically
 * allocated `struct sockaddr_storage`s.
 */
typedef struct {
  list_link_t link;                 // for the doubly linked lists
  int tcp_fd;                       // TCP connection socket
  int udp_fd;                       // UDP connection socket
  struct sockaddr_storage tcp_addr; // TCP address
  struct sockaddr_storage udp_addr; // UDP address
  socklen_t addr_len;  // address length; only difference is type + port
  int current_station; // currently connected station
} client_connection_t;

/**
 * Initializes a client connection given a TCP/UDP client sockets,
 * and their respective sockaddr information
 *
 * Inputs:
 * - client_connection_t *conn: the connection to initialize
 * - int tcp_fd: the TCP client's socket file descriptor
 * - int udp_fd: the UDP client's socket file descriptor
 * - struct sockaddr_storage tcp_sa: the TCP client's socket address
 * - struct sockaddr_storage tcp_sa: the UDP client's socket address
 * - socklen_t sa_len: the length of the socket addresses
 */
void init_connection(client_connection_t *conn, int tcp_fd, uint16_t udp_fd,
                     struct sockaddr *tcp_sa, struct sockaddr *udp_sa,
                     socklen_t sa_len);

/**
 * Destroys a dynamically initialized connection, closing the client file
 * descriptor and freeing dynamically allocated data.
 *
 * Inputs:
 * - client_connection_t *conn: the connection to free
 */
void destroy_connection(client_connection_t *conn);

#endif
