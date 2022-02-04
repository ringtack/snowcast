#ifndef __CLIENT_CONNECTION_H__
#define __CLIENT_CONNECTION_H__

#include "list.h"
#include "util.h"

typedef struct {
  list_link_t link;      // for the doubly linked lists
  int client_fd;         // TCP connection
  struct sockaddr *addr; // UDP address TODO: "localhost"?
  socklen_t addr_len;    // length of address
  uint16_t udp_port;     // UDP port
} client_connection_t;

/**
 * Initializes a client connection given a client socket, UDP port, and sockaddr
 *
 * Inputs:
 * - int client_fd: the client's socket file descriptor
 * - uint16_t udp_port: the UDP port of the client's listener
 * - struct sockaddr *sa: the client's socket address
 * - socklen_t sa_len: the length of the client's socket address
 *
 * Returns:
 * - A dynamically allocated client connection on success, NULL on failure
 */
client_connection_t *init_connection(int client_fd, uint16_t udp_port,
                                     struct sockaddr *sa, socklen_t sa_len);

/**
 * Destroys a dynamically initialized connection, closing the client file
 * descriptor and freeing dynamically allocated data (sockaddr, struct itself).
 *
 * Inputs:
 * - station_t *station: station to free
 *
 * Returns:
 * - 0 on success, non-zero on failure
 */
void destroy_connection(client_connection_t *conn);

#endif
