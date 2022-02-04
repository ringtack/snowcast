#include "snowcast_server.h"

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: ./snowcast_server <PORT>\n");
    exit(1);
  }

  test(argv);

  return 0;
}

void test(char *argv[]) {
  int sockfd = get_socket(NULL, argv[1], SOCK_STREAM);

  struct sockaddr_storage from_addr;
  socklen_t addr_len = sizeof(from_addr);
  int cfd = accept(sockfd, (struct sockaddr *)&from_addr, &addr_len);

  char ipstr[INET6_ADDRSTRLEN];
  get_addr_str(ipstr, (struct sockaddr *)&from_addr);
  printf("Received connection from %s.\n", ipstr);

  hello_t msg;
  int nbytes = recv(cfd, &msg, sizeof(msg), 0);
  printf("got %d bytes\n", nbytes);

  printf("Received hello: UDP port %d from client!\n", ntohs(msg.udp_port));

  welcome_t welcome = {REPLY_WELCOME, htons(258)};
  sendall(cfd, &welcome, sizeof(welcome));

  char *song = "Beethoven's 5th symphony";
  // char *song = "";
  uint16_t size = sizeof(announce_t) + strlen(song);
  announce_t *announce = malloc(size);
  announce->reply_type = REPLY_ANNOUNCE;
  announce->songname_size = strlen(song);
  memcpy(announce->songname, song, strlen(song));
  sendall(cfd, announce, size);
  free(announce);

  char *reply = "incorrect command!";
  invalid_command_t *invalid =
      malloc(sizeof(invalid_command_t) + strlen(reply));
  invalid->reply_type = REPLY_INVALID;
  invalid->reply_string_size = strlen(reply);
  memcpy(invalid->reply_string, reply, strlen(reply));
  sendall(cfd, invalid, sizeof(invalid_command_t) + strlen(reply));
  free(invalid);

  welcome.reply_type = 10;
  sendall(cfd, &welcome, sizeof(welcome));
}
