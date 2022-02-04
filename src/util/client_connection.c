#include "client_connection.h"

client_connection_t *init_connection(int client_fd, uint16_t udp_port,
                                     struct sockaddr *sa, socklen_t sa_len) {
  client_connection_t *conn = malloc(sizeof(client_connection_t));
  if (conn == NULL) {
    fprintf(stderr, "[init_connection] Failed to malloc client %d.\n",
            client_fd);
    return NULL;
  }

  list_link_init(&conn->link);
  conn->udp_port = udp_port;
  conn->client_fd = client_fd;
  conn->addr = malloc(sa_len);
  if (conn->addr == NULL) {
    fprintf(stderr,
            "[init_connection] Not enough memory for client %d's address.\n",
            client_fd);
    return NULL;
  }
  memcpy(conn->addr, sa, sa_len);
  conn->addr_len = sa_len;

  return conn;
}

void destroy_connection(client_connection_t *conn) {
  close(conn->client_fd);
  free(conn->addr);
  free(conn);
}
