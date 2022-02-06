#include "client_connection.h"

void init_client_vector(client_vector_t *client_vec, size_t max) {
  // check sanity on max
  assert(max > 0);

  // attempt to malloc an initial `max` spaces for connections and pfds
  client_vec->conns = malloc(max * sizeof(client_connection_t));
  if (client_vec->conns == NULL)
    fprintf(stderr, "[init_client_vector] Failed to malloc client conns.\n");

  client_vec->pfds = malloc(max * sizeof(struct pollfd));
  if (client_vec->pfds == NULL) {
    fprintf(stderr, "[init_client_vector] Failed to malloc pollfds.\n");
    // clean up
    free(client_vec->conns);
  }

  client_vec->size = 0;
  client_vec->max = max;
}

void destroy_client_vector(client_vector_t *client_vec) {
  // destroy information of each connection
  for (size_t i = 0; i < client_vec->size; i++)
    destroy_connection(&client_vec->conns[i]);

  // free vectors for conns and pfds
  free(client_vec->conns);
  free(client_vec->pfds);
}

int add_client(client_vector_t *client_vec, int client_fd, uint16_t udp_port,
               struct sockaddr *sa, socklen_t sa_len) {
  // check if we need more space
  if (client_vec->size == client_vec->max) {
    size_t new_max = (client_vec->max *= 2);

    // attempt to allocate more space for connections and pfds
    client_connection_t *new_conns =
        realloc(client_vec->conns, new_max * sizeof(*client_vec->conns));
    struct pollfd *new_pfds =
        realloc(client_vec->pfds, new_max * sizeof(*client_vec->pfds));

    // if either fails, restore back to old value; otherwise, set new values
    if ((new_conns == NULL) || (new_pfds == NULL)) {
      fprintf(stderr, "Failed to grow client conns/pfds. Restoring max...\n");
      client_vec->max /= 2;
      return -1;
    } else {
      client_vec->conns = new_conns;
      client_vec->pfds = new_pfds;
    }
  }

  size_t i = client_vec->size;
  // attempt to initialize a connection
  int ret =
      init_connection(&client_vec->conns[i], client_fd, udp_port, sa, sa_len);
  if (ret == -1) {
    return -1;
  }
  // create pollfd
  client_vec->pfds[i] = POLLFD(client_fd);

  // update size
  client_vec->size += 1;
  return 0;
}

void remove_client(client_vector_t *client_vec, int index) {
  // override current client with last client, then reduce count
  int size = client_vec->size;
  client_vec->conns[index] = client_vec->conns[size - 1];
  client_vec->pfds[index] = client_vec->pfds[size - 1];

  client_vec->size -= 1;
}

void resize_client_vector(client_vector_t *client_vec, int new_max) {
  size_t resize = 0;
  // if negative, check if we need to shrink
  if (new_max < 0) {
    // if sufficiently small, try to resize
    if (client_vec->size < (client_vec->max / 2))
      resize = client_vec->max / 2;
  } else {
    // if positive, check if new max satisfies constraints
    assert(new_max >= client_vec->size);
    resize = new_max;
  }

  // only resize if possible (i.e. resize != 0)
  if (resize) {
    // attempt to reallocate space for connections and pfds
    client_connection_t *new_conns =
        realloc(client_vec->conns, resize * sizeof(*client_vec->conns));
    struct pollfd *new_pfds =
        realloc(client_vec->pfds, resize * sizeof(*client_vec->pfds));

    // if either fails, don't update; otherwise, set new values
    if ((new_conns == NULL) || (new_pfds == NULL)) {
      fprintf(stderr,
              "[resize_client_vector] Failed to realloc client conns/pfds.\n");
    } else {
      client_vec->max = resize;
      client_vec->conns = new_conns;
      client_vec->pfds = new_pfds;
    }
  }
}

int init_connection(client_connection_t *conn, int client_fd, uint16_t udp_port,
                    struct sockaddr *sa, socklen_t sa_len) {
  // initialize link for potential use in linked lists
  list_link_init(&conn->link);

  conn->client_fd = client_fd;
  // attempt to malloc for tcp address
  conn->tcp_addr = malloc(sa_len);
  if (conn->tcp_addr == NULL) {
    fprintf(stderr,
            "[init_connection] Failed to malloc client %d's TCP address.\n",
            client_fd);
    return -1;
  }
  memcpy(conn->tcp_addr, sa, sa_len);

  // attempt to malloc for udp address; only difference is udp port
  conn->udp_addr = malloc(sa_len);
  if (conn->udp_addr == NULL) {
    fprintf(stderr,
            "[init_connection] Failed to malloc client %d's TCP address.\n",
            client_fd);
    // clean up
    free(conn->tcp_addr);
    return -1;
  }
  memcpy(conn->udp_addr, sa, sa_len);
  set_in_port(conn->udp_addr, udp_port);

  // same length for both addresses
  conn->addr_len = sa_len;

  return 0;
}

void destroy_connection(client_connection_t *conn) {
  close(conn->client_fd);
  free(conn->tcp_addr);
  free(conn->udp_addr);
}
