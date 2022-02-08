#ifndef __CLIENT_CONNECTION_H__
#define __CLIENT_CONNECTION_H__

#include "list.h"
#include "util.h"

/**
 * Struct representing a single client connection.
 * - client_fd is the TCP socket of the client connection. tcp_addr and udp_addr
 * are used to print information about the IP/port address of each connection,
 * if necessary; in addition, udp_addr is needed for the station sock_fd to send
 * information to.
 */
typedef struct {
  list_link_t link;                 // for the doubly linked lists
  int client_fd;                    // TCP connection socket
  struct sockaddr_storage tcp_addr; // TCP address
  struct sockaddr_storage udp_addr; // UDP address
  socklen_t addr_len;  // address length; only difference is type + port
  int current_station; // currently connected station
} client_connection_t;

/**
 * Initializes a client connection given a client socket, its sockaddr
 * information, and a UDP port
 *
 * Inputs:
 * - int client_fd: the TCP client's socket file descriptor
 * - uint16_t udp_port: the UDP's listening port
 * - struct sockaddr *sa: the TCP client's socket address
 * - socklen_t sa_len: the length of the socket addresses
 *
 * Returns:
 * - a dynamically allocated client connection, or NULL on failure
 */
client_connection_t *init_connection(int client_fd, uint16_t udp_port,
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
