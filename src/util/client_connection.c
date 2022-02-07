#include "client_connection.h"

void init_connection(client_connection_t *conn, int tcp_fd, uint16_t udp_fd,
                     struct sockaddr *tcp_sa, struct sockaddr *udp_sa,
                     socklen_t sa_len) {
  // initialize link for potential use in linked lists
  list_link_init(&conn->link);

  // set TCP info
  conn->tcp_fd = tcp_fd;
  memcpy(&conn->tcp_addr, tcp_sa, sa_len);

  // set UDP info
  conn->udp_fd = udp_fd;
  memcpy(&conn->udp_addr, udp_sa, sa_len);

  // same length for both addresses
  conn->addr_len = sa_len;
  conn->current_station = -1;
}

void destroy_connection(client_connection_t *conn) {
  fprintf(stderr, "Closing tcp fd [%d] and udp fd [%d]\n", conn->tcp_fd,
          conn->udp_fd);
  close(conn->tcp_fd);
  close(conn->udp_fd);
}
