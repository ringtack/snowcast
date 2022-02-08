#include "client_connection.h"

client_connection_t *init_connection(int client_fd, uint16_t udp_port,
                                     struct sockaddr *sa, socklen_t sa_len) {
  // attempt to allocate space for the client connection
  client_connection_t *conn = malloc(sizeof(client_connection_t));
  if (conn == NULL) {
    fprintf(stderr,
            "[init_connection] Failed to malloc space for connection %d.\n",
            client_fd);
    return NULL;
  }
  // initialize link for potential use in linked lists
  list_link_init(&conn->link);

  // set client info
  conn->client_fd = client_fd;
  memcpy(&conn->tcp_addr, sa, sa_len);

  // set UDP info
  memcpy(&conn->udp_addr, sa, sa_len);
  set_in_port((struct sockaddr *)&conn->udp_addr, udp_port);

  // same length for both addresses
  conn->addr_len = sa_len;
  conn->current_station = -1;

  return conn;
}

void destroy_connection(client_connection_t *conn) {
  fprintf(stderr, "Closing client fd [%d].\n", conn->client_fd);
  close(conn->client_fd);
  free(conn);
}
