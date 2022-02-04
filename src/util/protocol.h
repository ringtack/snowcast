#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include "util.h"

/*
 * COMMANDS
 */
#define MESSAGE_HELLO 0
#define MESSAGE_SET_STATION 1

typedef struct __attribute__((packed)) {
  uint8_t command_type;
  uint16_t udp_port;
} hello_t;

typedef struct __attribute__((packed)) {
  uint8_t command_type;
  uint16_t station_number;
} set_station_t;

/*
 * REPLIES
 */
#define REPLY_WELCOME 0
#define REPLY_ANNOUNCE 1
#define REPLY_INVALID 2

typedef struct __attribute__((packed)) {
  uint8_t reply_type;
  uint16_t num_stations;
} welcome_t;

typedef struct __attribute__((packed)) {
  uint8_t reply_type;
  uint8_t songname_size;
  char songname[];
} announce_t;

typedef struct __attribute__((packed)) {
  uint8_t reply_type;
  uint8_t reply_string_size;
  char reply_string[];
} invalid_command_t;

/**
 * Sends a command message.
 *
 * Inputs:
 * - int sockfd: the connection socket
 * - uint8_t cmd: the type of the command
 * - uint16_t val: the value of the command, IN HOST BYTE ORDER
 *
 * Returns:
 * - 0 on success, -1 on error
 */
int send_command_msg(int sockfd, uint8_t cmd, uint16_t val);

/**
 * Receives a reply message. YOU MUST FREE THE POINTER WHEN DONE!
 *
 * Inputs:
 * - int sockfd: the connection socket
 * - uint8_t *reply: a pointer to a reply type variable
 *
 * Returns:
 * - a `malloc`'d pointer to the reply struct. In addition, sets reply to the
 * appropriate type.
 */
void *recv_reply_msg(int sockfd, uint8_t *reply);

#endif
