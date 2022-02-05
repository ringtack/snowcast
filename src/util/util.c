#include "util.h"

void fatal_error(const char *msg) {
  perror(msg);
  exit(1);
}

void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  } else {
    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
  }
}

// get port
unsigned short get_in_port(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return ntohs(((struct sockaddr_in *)sa)->sin_port);
  } else {
    return ntohs(((struct sockaddr_in6 *)sa)->sin6_port);
  }
}

void get_addr_str(char ipstr[INET6_ADDRSTRLEN], struct sockaddr *sa) {
  if (inet_ntop(sa->sa_family, get_in_addr(sa), ipstr, INET6_ADDRSTRLEN) ==
      NULL) {
    fatal_error("inet_ntop");
  }
}

void get_address(char buf[], struct sockaddr *sa) {
  char ipstr[INET6_ADDRSTRLEN];
  if (inet_ntop(sa->sa_family, get_in_addr(sa), ipstr, INET6_ADDRSTRLEN) ==
      NULL)
    fatal_error("inet_ntop");
  unsigned short port = get_in_port(sa);
  sprintf(buf, "%s:%d", ipstr, port);
}

int sendall(int sockfd, void *val, int len) {
  int total = 0;
  int bytesleft = len;
  int n;
  // while bytes sent < total bytes, attempt sending the rest
  while (total < len) {
    n = send(sockfd, val + total, bytesleft, 0);
    // if an error occurs while sending, print error and return -1
    if (n == -1) {
      perror("sendall: send");
      return n;
    }
    // otherwise, update counts
    total += n;
    bytesleft -= n;
  }
  return 0;
}

int sendtoall(int sockfd, void *val, int len, struct sockaddr *sa,
              socklen_t sa_len) {
  int total = 0;
  int bytesleft = len;
  int n;
  // while bytes sent < total bytes, attempt sending the rest
  while (total < len) {
    n = sendto(sockfd, val + total, bytesleft, 0, sa, sa_len);
    // if an error occurs while sending, return -1
    if (n == -1) {
      perror("sendtoall: sendto");
      return n;
    }
    // otherwise, update counts
    total += n;
    bytesleft -= n;
  }
  return 0;
}

int recvall(int sockfd, void *buf, int len) {
  // TODO: timeout starting from first attempt vs. restarting on every attempt?
  // Nick said it's ok to estart on every attempt; confirm with Staff later
  // configure socket to timeout on 100ms

  // alternatively, use alarm(2)?
  struct timeval timeout = {0, 100 * 1000};
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) ==
      -1) {
    perror("recvall: setsockopt");
    return -1;
  }

  int total = 0;
  int bytesleft = len;
  int n;
  // while bytes received < total bytes expected, listen for the rest
  while (total < len) {
    n = recv(sockfd, buf + total, bytesleft, 0);
    // display if error
    if (n == -1) {
      perror("recvall: recv");
      return -1;
    }
    // display if server disconnected
    if (n == 0) {
      fprintf(stderr, "Server closed the connection or disconnected!\n");
      return 1;
    }
    // otherwise, update counts
    total += n;
    bytesleft -= n;
  }
  return 0;
}

int get_socket(const char *hostname, const char *port, int socktype) {
  struct addrinfo hints, *res, *r;
  // set hints
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = socktype;
  // if no hostname, use our own
  if (!hostname)
    hints.ai_flags = AI_PASSIVE;

  // get address info
  int ret;
  if ((ret = getaddrinfo(hostname, port, &hints, &res)) == -1) {
    fprintf(stderr, "[get_socket] getaddrinfo error: %s\n", gai_strerror(ret));
    return -1;
  }

  int yes = 1;
  int sockfd;
  char ipstr[INET6_ADDRSTRLEN];
  char buf[MAXBUFSIZ];
  // loop over results, binding to first possible
  for (r = res; r; r = r->ai_next) {
    // attempt socket creation
    if ((sockfd = socket(r->ai_family, r->ai_socktype, r->ai_protocol)) == -1)
      continue; // not an error!

    // if server, attempt to bind
    if (!hostname) {
      // allow port re-use
      if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
        continue; // not an error!

      // attempt to bind
      if (bind(sockfd, r->ai_addr, r->ai_addrlen) != -1)
        break;

      // otherwise, if TCP, attempt to connect
    } else if (socktype == SOCK_STREAM) {
      if (connect(sockfd, r->ai_addr, r->ai_addrlen) == 0) {
        get_addr_str(ipstr, r->ai_addr);
        printf("Resolving %s -> %s...\n", hostname, ipstr);
        get_address(buf, r->ai_addr);
        printf("Connected to server %s.\n", buf);
        break;
      }
      // not an error if fail to connect!
    }
  }

  // if no connection works, display error, free address info and exit
  if (r == NULL) {
    fprintf(stderr, "[get_socket] No available ports on %s!\n", port);
    freeaddrinfo(res);
    return -1;
  }

  // if TCP and server, configure listening
  if (!hostname) {
    if (socktype == SOCK_STREAM)
      if (listen(sockfd, BACKLOG) == -1) {
        perror("get_socket: listen");
        freeaddrinfo(res);
        return -1;
      }
    get_address(buf, r->ai_addr);
    printf("[Server] Listening on %s...\n", buf);
  }

  // free address info
  freeaddrinfo(res);

  return sockfd;
}
