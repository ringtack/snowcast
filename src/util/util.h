#ifndef __UTIL_H__
#define __UTIL_H__

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "list.h"

#define BACKLOG 20
#define MAXBUFSIZ 256

#define POLLFD(new_fd)                                                         \
  (struct pollfd) { .fd = new_fd, .events = POLLIN }

/**
 * Prints the error associated with 'errno', and exits.
 *
 * Inputs:
 * - char *msg: string of the appropriate function
 */
void fatal_error(const char *msg);

/**
 * Retrieves the internet address of a struct sockaddr, depending on its family.
 *
 * Inputs:
 * - struct sockaddr *sa: the sockaddr of interest
 *
 * Returns:
 * - a pointer to the appropriate internet address (either sin_addr or
 * sin6_addr)
 */
void *get_in_addr(struct sockaddr *sa);

/**
 * Retrieves the port of a struct sockaddr, depending on its family.
 *
 * Inputs:
 * - struct sockaddr *sa: the sockaddr of interest
 *
 * Returns:
 * - the port of the internet address
 */
unsigned short get_in_port(struct sockaddr *sa);

/**
 * Converts the internet address of a struct sockaddr into a string.
 *
 * Inputs:
 * - char ipstr[INET6_ADDRSTRLEN]: a buffer of size INET6_ADDRSTRLEN to store
 * the string
 * - struct sockaddr *sa: the sockaddr of interest
 */
void get_addr_str(char ipstr[INET6_ADDRSTRLEN], struct sockaddr *sa);

/**
 * Creates a string representation of the IP:Port of a struct sockaddr*.
 *
 * Inputs:
 * - char buf[]: a buffer of sufficient size to store the address
 * - struct sockaddr *sa: the sockaddr of interest
 */
void get_address(char buf[], struct sockaddr *sa);

/**
 * Utility function to send all bytes of a value (TCP).
 *
 * Inputs:
 * - int sockfd: the connection socket
 * - void *val: the value to send
 * - int len: the size of the value
 *
 * Returns:
 * - 0 if all data was successfully sent, -1 if an error occurred
 */
int sendall(int sockfd, void *val, int len);

/**
 * Utility function to receive all bytes of a value (TCP).
 *
 * Inputs:
 * - int sockfd: the connection socket
 * - void *buf: where to receive the information
 * - int len: the size of the expected value
 *
 * Returns:
 * - 0 if all data was successfully received, -1 if an error occurred, 1 if the
 * server disconnected
 */
int recvall(int sockfd, void *buf, int len);

/**
 * Given a hostname and port, attempts to open a socket.
 *
 * Inputs:
 * - const char *hostname: desired hostname, or NULL if listener socket
 * - const char *port: desired port, or NULL if irrelevant
 * - int socktype: the desired socktype (UDP or TCP)
 *
 * Returns:
 * - the listener socket fd, or -1 if error
 */
int get_socket(const char *hostname, const char *port, int socktype);

#endif
