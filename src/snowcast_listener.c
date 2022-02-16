#include "snowcast_listener.h"

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: ./snowcast_listener <PORT>\n");
    exit(1);
  }
  const char *port = argv[1];
  // host name doesn't matter
  int udp_fd = get_socket(NULL, port, SOCK_DGRAM);

  if (udp_fd == -1) {
    exit(1);
  }

  char buf[BSIZ];
  while (1) {
    // we don't care about receiving all the information or knowing where it
    // came from
    memset(buf, 0, sizeof(buf));
    int ret = recvfrom(udp_fd, buf, BSIZ, 0, NULL, NULL);
    if (ret == -1) {
      perror("recvfrom");
      exit(1);
    }

    // print all information received
    fwrite(buf, sizeof(char), BSIZ, stdout);
  }

  return 0;
}
