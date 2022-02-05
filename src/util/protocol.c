#include "protocol.h"

int send_command_msg(int sockfd, uint8_t cmd, uint16_t val) {
  // convert to Network Byte Order
  val = htons(val);
  if (cmd == MESSAGE_HELLO) {
    hello_t msg = {cmd, val};
    int len = sizeof(msg);
    if (sendall(sockfd, &msg, len))
      return -1;
  } else if (cmd == MESSAGE_SET_STATION) {
    set_station_t msg = {cmd, val};
    if (sendall(sockfd, &msg, sizeof(msg)))
      return -1;
  } else {
    // currently, only supports Hello and SetStation
    fprintf(stderr, "Invalid command type [%d]!\n", cmd);
    return -1;
  }
  return 0;
}

/*
 * A note on implementation: currently, all command messages are just three
 * bytes long; thus, it is possible just to read three bytes at once and convert
 * from there. I choose to read the command type first, then read the rest, in
 * case I want to extend the Snowcast protocol for different commands.
 */
void *recv_command_msg(int sockfd, uint8_t *reply) {
  // read in command type; only 1 byte
  if (recvall(sockfd, reply, sizeof(*reply)) == -1)
    return NULL;

  if (*reply == MESSAGE_HELLO) {
    // if Hello, read in two bytes for the UDP port
    uint16_t udp_port;
    if (recvall(sockfd, &udp_port, sizeof(udp_port)) == -1)
      return NULL;
    // create dynamic pointer to Welcome message, and set values
    hello_t *hello = malloc(sizeof(hello_t));
    hello->command_type = *reply;
    hello->udp_port = udp_port;
    return hello;
  } else if (*reply == MESSAGE_SET_STATION) {
    // if Set Station, read in two bytes for the desired station
    uint16_t station_number;
    if (recvall(sockfd, &station_number, sizeof(station_number)) == -1)
      return NULL;
    set_station_t *set_station = malloc(sizeof(set_station_t));
    set_station->command_type = *reply;
    set_station->station_number = station_number;
    return set_station;
  } else {
    // Otherwise, not a valid message, so indicate as such
    return NULL;
  }
}

int send_reply_msg(int sockfd, uint8_t cmd, uint16_t val, const char *msg) {
  if (cmd == REPLY_WELCOME) {
    welcome_t welcome = {cmd, htons(val)};
    int len = sizeof(welcome);
    if (sendall(sockfd, &welcome, len))
      return -1;
  } else if (cmd == REPLY_ANNOUNCE || cmd == REPLY_INVALID) {
    // since they're the same structure, we follow the same procedures for both.
    // TODO: change if we want to adjust spec
    size_t size = sizeof(announce_t) + strlen(msg);
    announce_t *announce = malloc(size);
    if (announce == NULL) {
      fprintf(stderr, "[send_reply_msg] Could not malloc command %d.\n", cmd);
      return -1;
    }
    announce->reply_type = cmd;
    announce->songname_size = htons((uint8_t)val);
    memcpy(announce->songname, msg, strlen(msg));
    if (sendall(sockfd, announce, size)) {
      free(announce);
      return -1;
    }
    free(announce);
  } else {
    fprintf(stderr, "[send_reply_msg] Invalid command type %d.\n", cmd);
    return -1;
  }
  return 0;
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
    // create dynamic pointer to Welcome message, and set values
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
