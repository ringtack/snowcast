#include "protocol.h"
int send_command_msg(int sockfd, uint8_t cmd, uint16_t val) {
  // convert to Network Byte Order
  val = htons(val);
  int ret;
  if (cmd == MESSAGE_HELLO) {
    hello_t msg = {cmd, val};
    int len = sizeof(msg);
    ret = sendall(sockfd, &msg, len);
  } else if (cmd == MESSAGE_SET_STATION) {
    set_station_t msg = {cmd, val};
    ret = sendall(sockfd, &msg, sizeof(msg));
  } else {
    // currently, only supports Hello and SetStation
    fprintf(stderr, "Invalid command type [%d]!\n", cmd);
    return -1;
  }
  // returns 0 on success, -1 on failure
  return ret;
}

void *recv_reply_msg(int sockfd, uint8_t *reply) {
  // read in reply type; only 1 byte
  if (recvall(sockfd, reply, sizeof(*reply)) == -1)
    return NULL;

  uint8_t size;
  if (*reply == REPLY_WELCOME) {
    // if Welcome, read in two bytes for number of stations
    uint16_t num_stations;
    if (recvall(sockfd, &num_stations, sizeof(num_stations)) == -1)
      return NULL;
    // create dynamic pointer to Welcome message, and setvalues
    welcome_t *welcome = malloc(sizeof(welcome_t));
    welcome->reply_type = *reply;
    welcome->num_stations = ntohs(num_stations); // remember to convert to HBO!
    return welcome;
  } else if (*reply == REPLY_ANNOUNCE || *reply == REPLY_INVALID) {
    // if Announce or Invalid, read in size of string
    if (recvall(sockfd, &size, sizeof(size)) == -1) {
      return NULL;
    }

    // Then, read in contents of string
    char msg[size];
    if (recvall(sockfd, msg, size * sizeof(char)) == -1) {
      return NULL;
    }
    // create dynamic pointers and initialize values
    if (*reply == REPLY_ANNOUNCE) {
      announce_t *announce =
          malloc(sizeof(announce_t) + (size + 1) * sizeof(char));
      announce->reply_type = *reply;
      announce->songname_size = size;
      memcpy(announce->songname, msg, size);
      announce->songname[size] = '\0'; // need size + 1 for null terminator!
      return announce;
    } else {
      invalid_command_t *invalid_command =
          malloc(sizeof(invalid_command_t) + (size + 1) * sizeof(char));
      invalid_command->reply_type = *reply;
      invalid_command->reply_string_size = size;
      memcpy(invalid_command->reply_string, msg, size);
      invalid_command->reply_string[size] = '\0';
      return invalid_command;
    }
  } else {
    // otherwise, not a valid command, so indicate as such
    return NULL;
  }
}
