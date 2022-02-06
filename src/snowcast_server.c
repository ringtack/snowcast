#include "snowcast_server.h"

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(
        stderr,
        "Usage: ./snowcast_server <PORT> <FILE1> [<FILE2> [<FILE3> [...]]]\n");
    exit(1);
  }

  //// INITIALIZATION ////
  // open listener socket
  int listener = get_socket(NULL, argv[1], SOCK_STREAM);
  if (listener == -1)
    exit(1); // get_socket handles error printing

  int ret;
  // Initialize client, server, and station control structs
  // clean up everything on failure
  // TODO: do I actually need the destroy_struct... calls? so tedious........
  server_control_t server_control;
  if ((ret = init_server_control(&server_control, INIT_NUM_THREADS))) {
    close(listener);
    exit(1);
  }

  station_control_t station_control;
  size_t num_stations = argc - 2;
  char **songs = argv + 2;
  if ((ret = init_station_control(&station_control, num_stations, songs))) {
    close(listener);
    exit(1);
  }

  client_control_t client_control;
  if ((ret = init_client_control(&client_control))) {
    close(listener);
    exit(1);
  }

  // TODO: ACCEPT CLIENTS IN A LOOP
  // -> PUT INTO STATIONS AND POLLING FDs
  // -> CHECK IF SENDING DATA WORKS

  //// CLEANUP ////

  // TODO: DESTROY SNOWCAST SERVER STRUCT
  // TODO: DESTROY CLIENT CONTROL STRUCT

  return 0;
}

/* ===============================================================================
 *                              HELPER FUNCTIONS
 * ===============================================================================
 */

// So this doesn't actually return -1 on failure, it just exits. This is fine
// since it's the first init call, but I may want to change how I report errors
// here in the future.
int init_server_control(server_control_t *server_control, size_t num_threads) {
  // initialize synchronization primitives
  int ret = pthread_mutex_init(&server_control->server_mtx, NULL) ||
            pthread_cond_init(&server_control->server_cond, NULL);
  if (ret)
    handle_error_en(ret, "init_server_control: pthread_{mutex,cond}_init");

  // attempt to create thread pool
  server_control->t_pool = init_thread_pool(num_threads);
  if (server_control->t_pool == NULL) {
    // cleanup on failure
    ret = pthread_mutex_destroy(&server_control->server_mtx) ||
          pthread_cond_destroy(&server_control->server_cond);
    if (ret)
      handle_error_en(ret, "init_server_control: pthread_{mutex,cond}_destroy");
  }
  server_control->stopped = 1;

  return 0;
}

// TODO: FIX UP, I NEED TO HANDLE CLEANUP
void destroy_server_control(server_control_t *server_control) {
  // lock access to server control
  pthread_mutex_lock(&server_control->server_mtx);

  // set stopped to 1, then destroy thread pool
  server_control->stopped = 1;
  destroy_thread_pool(server_control->t_pool);

  // unlock access, then destroy synchronization primitives
  pthread_mutex_unlock(&server_control->server_mtx);
  int ret = pthread_mutex_destroy(&server_control->server_mtx) ||
            pthread_cond_destroy(&server_control->server_cond);
  if (ret)
    handle_error_en(ret, "init_server_control: pthread_{mutex,cond}_destroy");
}

int init_station_control(station_control_t *station_control,
                         size_t num_stations, char *songs[]) {
  // attempt to malloc enough space for the stations
  station_control->stations = malloc(sizeof(station_t *));
  if (station_control->stations == NULL) {
    fprintf(stderr,
            "[init_station_control] Could not malloc stations array.\n");
    return -1;
  }

  station_control->num_stations = num_stations;
  // attempt to init every station
  for (size_t i = 0; i < num_stations; i++) {
    station_control->stations[i] = init_station(i, songs[i]);
    if (station_control->stations[i] == NULL) {
      // cleanup previously initialized stations
      for (int j = 0; j < i; j++)
        destroy_station(station_control->stations[i]);
      free(station_control->stations);
      return -1;
    }
  }

  // initialize mutex
  int ret = pthread_mutex_init(&station_control->station_mtx, NULL);
  if (ret) {
    // TODO: print better output
    fprintf(stderr, "[init_station_control] Failed to initialize mutex.\n");
    // if failure, cleanup previously initialized stations
    for (size_t i = 0; i < num_stations; i++)
      destroy_station(station_control->stations[i]);
    free(station_control->stations);
    return -1;
  }

  return 0;
}

void destroy_station_control(station_control_t *station_control) {
  // lock access to prevent others from editing stations while destroying
  pthread_mutex_lock(&station_control->station_mtx);
  // cleanup all stations
  for (size_t i = 0; i < station_control->num_stations; i++)
    destroy_station(station_control->stations[i]);
  // free stations array
  free(station_control->stations);

  // unlock and destroy mutex
  pthread_mutex_unlock(&station_control->station_mtx);
  int ret = pthread_mutex_destroy(&station_control->station_mtx);
  if (ret) {
    // TODO: print better output
    fprintf(stderr, "[destroy_station_control] Failed to destroy mutex.\n");
  }
}

int init_client_control(client_control_t *client_control) {
  int ret = init_client_vector(&client_control->client_vec, INIT_MAX_CLIENTS);
  if (ret)
    return -1;
  if ((ret = pthread_mutex_init(&client_control->clients_mtx, NULL))) {
    // TODO: print better output
    fprintf(stderr, "[init_station_control] Failed to init mutex.\n");
    destroy_client_vector(&client_control->client_vec);
    return -1;
  }

  return 0;
}

void destroy_client_control(client_control_t *client_control) {
  destroy_client_vector(&client_control->client_vec);
  int ret = pthread_mutex_destroy(&client_control->clients_mtx);
  if (ret) {
    // TODO: print better output
    fprintf(stderr, "[destroy_station_control] Failed to destroy mutex.\n");
  }
}

// RANDOM TEST STUFF SHOULD REMOVE EVENTUALLY
/* void test(char *argv[]) { */
/* int sockfd = get_socket(NULL, argv[1], SOCK_STREAM); */

/* struct sockaddr_storage from_addr; */
/* socklen_t addr_len = sizeof(from_addr); */
/* int cfd = accept(sockfd, (struct sockaddr *)&from_addr, &addr_len); */

/* char ipstr[INET6_ADDRSTRLEN]; */
/* get_addr_str(ipstr, (struct sockaddr *)&from_addr); */
/* printf("Received connection from %s.\n", ipstr); */

/* hello_t msg; */
/* int nbytes = recv(cfd, &msg, sizeof(msg), 0); */
/* printf("got %d bytes\n", nbytes); */

/* printf("Received hello: UDP port %d from client!\n", ntohs(msg.udp_port));
 */

/* welcome_t welcome = {REPLY_WELCOME, htons(258)}; */
/* sendall(cfd, &welcome, sizeof(welcome)); */

/* char *song = "Beethoven's 5th symphony"; */
/* // char *song = ""; */
/* uint16_t size = sizeof(announce_t) + strlen(song); */
/* announce_t *announce = malloc(size); */
/* announce->reply_type = REPLY_ANNOUNCE; */
/* announce->songname_size = strlen(song); */
/* memcpy(announce->songname, song, strlen(song)); */
/* sendall(cfd, announce, size); */
/* free(announce); */

/* char *reply = "incorrect command!"; */
/* invalid_command_t *invalid = */
/* malloc(sizeof(invalid_command_t) + strlen(reply)); */
/* invalid->reply_type = REPLY_INVALID; */
/* invalid->reply_string_size = strlen(reply); */
/* memcpy(invalid->reply_string, reply, strlen(reply)); */
/* sendall(cfd, invalid, sizeof(invalid_command_t) + strlen(reply)); */
/* free(invalid); */

/* welcome.reply_type = 10; */
/* sendall(cfd, &welcome, sizeof(welcome)); */
/* } */

/* typedef struct { */
/* int listener; */
/* int num_stations; */
/* } accept_args_t; */

/* void *accept_client(void *arg) { */
/* accept_args_t *args = (accept_args_t *)arg; */
/* int listener = args->listener; */
/* int num_stations = args->num_stations; */

/* // accept client connection */
/* struct sockaddr_storage from_addr; */
/* socklen_t addr_len = sizeof(from_addr); */
/* int cfd = accept(listener, (struct sockaddr *)&from_addr, &addr_len); */
/* // print information */
/* char ipstr[INET6_ADDRSTRLEN]; */
/* get_addr_str(ipstr, (struct sockaddr *)&from_addr); */
/* printf("Received connection from %s.\n", ipstr); */

/* // wait for hello message */
/* uint8_t type; */
/* void *msg = recv_command_msg(cfd, &type); */

/* if (type == MESSAGE_HELLO) { */
/* hello_t *hello = (hello_t *)msg; */
/* printf("Received hello: UDP port %d from client!\n", */
/* ntohs(hello->udp_port)); */

/* printf("Sending Welcome to client %s...\n", ipstr); */
/* if (send_reply_msg(cfd, REPLY_WELCOME, num_stations, NULL)) { */
/* fprintf(stderr, "Could not send Welcome to client %s.\n", ipstr); */
/* exit(1); */
/* } */
/* } else { */
/* fprintf(stderr, "Did not receive first Hello from client %s.\n", ipstr); */
/* // TODO: OTHER CLEANUP */
/* exit(1); */
/* } */

/* printf("Done.\n"); */
/* free(msg); */

/* return NULL; */
/* } */
