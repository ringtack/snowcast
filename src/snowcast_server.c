#include "snowcast_server.h"

// LMAO THIS IS KINDA SCUFFED BUT MVP AMIRITE
// fr tho I'll flesh out the stuff I have most components I just need to put
// stuff together

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(
        stderr,
        "Usage: ./snowcast_server <PORT> <FILE1> [<FILE2> [<FILE3> [...]]]\n");
    exit(1);
  }

  // TODO: INITIALIZE SNOWCAST SERVER STRUCT
  // TODO: INITIALIZE CLIENT CONTROL STRUCT
  int num_stations = argc - 2;
  station_t *stations[num_stations];
  // initialize all stations
  for (int i = 0; i < num_stations; i++) {
    char *song_name = argv[i + 2];
    printf("Station %d is now playing \"%s\".\n", i, song_name);
    stations[i] = init_station(i, song_name);
  }

  // open listener socket
  int listener = get_socket(NULL, argv[1], SOCK_STREAM);
  if (listener == -1)
    exit(1); // get_socket handles error printing

  // accept client connection
  struct sockaddr_storage from_addr;
  socklen_t addr_len = sizeof(from_addr);
  int cfd = accept(listener, (struct sockaddr *)&from_addr, &addr_len);
  // print information
  char ipstr[INET6_ADDRSTRLEN];
  get_addr_str(ipstr, (struct sockaddr *)&from_addr);
  printf("Received connection from %s.\n", ipstr);

  // wait for hello message
  uint8_t type;
  void *msg = recv_command_msg(cfd, &type);

  if (type == MESSAGE_HELLO) {
    hello_t *hello = (hello_t *)msg;
    printf("Received hello: UDP port %d from client!\n",
           ntohs(hello->udp_port));

    printf("Sending Welcome to client %s...\n", ipstr);
    if (send_reply_msg(cfd, REPLY_WELCOME, num_stations, NULL)) {
      fprintf(stderr, "Could not send Welcome to client %s.\n", ipstr);
      exit(1);
    }
  } else {
    fprintf(stderr, "Did not receive first Hello from client %s.\n", ipstr);
    // TODO: OTHER CLEANUP
    exit(1);
  }

  printf("Done.\n");
  free(msg);
  // TODO: ACCEPT CLIENTS IN A LOOP
  // -> PUT INTO STATIONS AND POLLING FDs
  // -> CHECK IF SENDING DATA WORKS

  //// CLEANUP ////
  // free stations
  for (int i = 0; i < num_stations; i++)
    destroy_station(stations[i]);

  // TODO: DESTROY SNOWCAST SERVER STRUCT
  // TODO: DESTROY CLIENT CONTROL STRUCT

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
