#include "client_vector.h"

int init_client_vector(client_vector_t *client_vec, size_t max, int listener) {
  // check sanity on max
  assert(max > 0);

  // attempt to malloc an initial `max` spaces for connections
  client_vec->conns = malloc(max * sizeof(client_connection_t *));
  if (client_vec->conns == NULL) {
    fprintf(stderr, "[init_client_vector] Failed to malloc client conns.\n");
    return -1;
  }

  // attempt to malloc an initial `max + 1` spaces for pollfds
  client_vec->pfds = malloc((max + 1) * sizeof(struct pollfd));
  if (client_vec->pfds == NULL) {
    fprintf(stderr, "[init_client_vector] Failed to malloc pollfds.\n");
    // clean up
    free(client_vec->conns);
    return -1;
  }

  // set first pfd to the listener socket
  client_vec->pfds[0] = POLLFD(listener);

  client_vec->size = 0;
  client_vec->max = max;
  client_vec->listener = listener;

  return 0;
}

void destroy_client_vector(client_vector_t *client_vec) {
  // destroy information of each connection
  for (size_t i = 0; i < client_vec->size; i++)
    destroy_connection(client_vec->conns[i]);

  // free vectors for conns and pfds
  free(client_vec->conns);
  free(client_vec->pfds);
  close(client_vec->listener);
}

int add_client(client_vector_t *client_vec, int client_fd, uint16_t udp_port,
               struct sockaddr *sa, socklen_t sa_len) {
  // check if we need more space
  if (client_vec->size == client_vec->max) {
    // attempt to resize; if fail, indicate
    int ret = resize_client_vector(client_vec, 2 * client_vec->max);
    if (ret != 0) {
      fprintf(stderr, "[add_client] Failed to resize vector. See above.\n");
      return -1;
    }
  }

  size_t i = client_vec->size;
  // initialize a connection
  client_vec->conns[i] = init_connection(client_fd, udp_port, sa, sa_len);
  if (client_vec->conns[i] == NULL) {
    fprintf(stderr, "[add_client] Failed to add client. See above.\n");
    return -1;
  }

  // create pollfd for TCP client
  client_vec->pfds[i + 1] = POLLFD(client_fd);

  // update size
  client_vec->size += 1;
  return i;
}

void remove_client(client_vector_t *client_vec, int index) {
  client_connection_t *old_conn = client_vec->conns[index];
  // override current client with last client, then reduce count
  int size = client_vec->size;
  client_vec->conns[index] = client_vec->conns[size - 1];
  client_vec->pfds[index + 1] = client_vec->pfds[size];
  client_vec->size -= 1;

  // destroy connection
  destroy_connection(old_conn);
}

client_connection_t *get_client(client_vector_t *client_vec, int index) {
  if (index >= client_vec->size) {
    fprintf(stderr,
            "[get_client] Index out of bounds [requested: %d, size: %zu]\n",
            index, client_vec->size);
    return NULL;
  }

  return client_vec->conns[index];
}

int resize_client_vector(client_vector_t *client_vec, int new_max) {
  size_t resize = 0;
  // if negative, check if we need to shrink
  if (new_max < 0) {
    // if sufficiently small, try to half
    if (client_vec->size < (client_vec->max / 2))
      resize = client_vec->max / 2;
  } else {
    // if positive, check if new max satisfies constraints
    assert(new_max >= client_vec->size);
    if (new_max < client_vec->size) {
      fprintf(
          stderr,
          "[resize_client_vector] Max must be larger than the current size.\n");
      return -1;
    }
    resize = new_max;
  }

  // only resize if possible (i.e. resize != 0)
  if (resize) {
    // attempt to reallocate space for connections and pfds
    client_connection_t **new_conns =
        realloc(client_vec->conns, resize * sizeof(client_connection_t *));
    struct pollfd *new_pfds =
        realloc(client_vec->pfds, (resize + 1) * sizeof(*client_vec->pfds));

    // if either fails, don't update; otherwise, set new values
    if ((new_conns == NULL) || (new_pfds == NULL)) {
      fprintf(stderr,
              "[resize_client_vector] Failed to realloc client conns/pfds.\n");
      return -1;
    } else {
      client_vec->max = resize;
      client_vec->conns = new_conns;
      client_vec->pfds = new_pfds;
    }
  }

  return 0;
}
