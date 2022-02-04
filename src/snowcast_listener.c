#include "snowcast_listener.h"

int main(int argc, char *argv[]) {
  if (argc != 4) {
    fprintf(
        stderr,
        "Usage: ./snowcast_control <SERVER_NAME> <SERVER_PORT> <UDP_PORT>\n");
    exit(1);
  }

  test(argv);

  return 0;
}

void test(char *argv[]) {
  int sockfd = get_socket(argv[1], argv[2], SOCK_STREAM);
  if (sockfd == -1) {
    fprintf(stderr, "could not connect!\n");
    exit(1);
  }

  uint16_t port = atoi(argv[3]);
  printf("UDP port is %d\n", port);
  send_command_msg(sockfd, MESSAGE_HELLO, port);

  for (int i = 0; i < 4; i++) {

    uint8_t type;
    void *reply = recv_reply_msg(sockfd, &type);

    switch (type) {
    case REPLY_WELCOME: {
      welcome_t *welcome = (welcome_t *)reply;
      printf("Welcome to Snowcast! There are %d stations.\n",
             welcome->num_stations);
      break;
    }
    case REPLY_ANNOUNCE: {
      announce_t *announcement = (announce_t *)reply;
      printf("Anouncement: switched to song \"%s\"\n", announcement->songname);
      break;
    }
    case REPLY_INVALID: {
      invalid_command_t *invalid = (invalid_command_t *)reply;
      fprintf(stderr, "You sent an invalid command! Response: \"%s\"\n",
              invalid->reply_string);
      break;
    }
    default: {
      fprintf(stderr, "The server sent an invalid reply!\n");
      break;
    }
    }
    free(reply);
  }
}
