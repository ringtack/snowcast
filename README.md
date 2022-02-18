# Snowcast

NOTE: I am using a late day on this! Please use my latest commit before 11:59PM EST, 2/17.

Welcome to Snowcast, the 100% secure streaming service fitted with a wide variety of tunes. Poke
around, and please don't crash the server (ˆ ﻌ ˆ)♡

## Usage

Executables can be created and deleted with `make all` and `make clean` respectively. Each
executable provides usage instructions, but in short:

```
- ./snowcast_server <PORT> FILE1 [FILE2 [FILE3 ...]]
    - <PORT> specifies the port on which the server should listen.
    - FILE1 [FILE2 [FILE3 ...]] specify which songs the server's stations should stream. At least
    one song is required, but you may specify as many as you wish.
- ./snowcast_control <SERVERNAME> <SERVERPORT> <LISTENER_PORT>
    - <SERVERNAME> and <SERVERPORT> specify the IP address and port of the snowcast server,
    respectively. In most use cases, SERVERNAME will be localhost.
    - <LISTENER_PORT> is the port on which a UDP listener will listen.
- ./snowcast_listener <PORT>
    - <PORT> specifies the port on which a client listener will listen for streamed information.
```

## Snowcast Server

### Control Structures

The server consists of three central control structures, each of which is extensively described in
inline documentation; here is a brief overview of each's functionality.

#### `server_control_t`

```c
typedef struct {
  thread_pool_t *t_pool;      // thread pool for polling work!
  pthread_mutex_t server_mtx; // synchronize access to server
  pthread_cond_t server_cond; // condition variable for cleanup
  uint8_t stopped;            // flag for server condition
} server_control_t;

```

A `server_control_t` instance handles the primary operations of the server: a thread pool `t_pool`
manages all "work" a server must perform in response to clients (i.e. accepting clients and
responding to client commands).

> In other worlds, I also had the thread pool handle station streaming, but I decided against it
> rather arbitrarily.

Because `t_pool` already handles synchronizing adding jobs to the thread pool, `server_mtx` and
`server_cond`, together with `stopped`, are instead used solely for cleanup. When the server REPL
receives a `q` input (or a fatal error occurs during server operations), the `stopped` flag is set
to `1`, and the server waits on `server_cond` until it is ready for complete cleanup of the server.

#### `station_control_t`

```c
typedef struct {
  station_t **stations;        // available stations
  size_t num_stations;         // keeps track of the number of stations
  pthread_mutex_t station_mtx; // mutex for station control access
} station_control_t;
```

A `station_control_t` instance handles all operations involving stations, i.e. adding, removing, and
swapping clients from stations. A `station_mtx` is used to synchronize these operations, which may
be an issue if multiple clients requested swapping at the same time.

Currently, `stations` and `num_stations` are static, in that the values are not changed after
initialization. However, if time permits, I hope to expand the server's station capabilites by
allowing the server to add/remove stations from the list, and manually change which songs are being
played. Because `stations` is a dynamically allocated array, and every access to the
`station_control_t` structure is synchronized, there should be no issues with regards to
concurrently modifying the stations structure.

The `station_t` structure will be described in detail below.

#### `client_control_t`

```c
typedef struct {
  client_vector_t client_vec;  // vector of currently connected clients
  size_t num_pending;          // record number of ops that change client_vec
  pthread_mutex_t clients_mtx; // synchronize access to client control
  pthread_cond_t pending_cond; // wait until no more pending
  // TODO: implement a signal handler
} client_control_t;
```

A `client_control_t` instance handles all operations involving clients: adding/removing clients upon
connection, polling for client requests, and synchronizing changes between clients and stations.

`client_vec` stores a dynamically sized array of client connection information (described in detail
below). In order to synchronize access to the client vector and prevent data races when the server
is `poll`ing for requests, `clients_mtx` is locked whenever the structure is accessed, `num_pending`
records the number of client-related operations on the thread pool, and `pending_cond` provides a
way for the client request listener thread to wait until all client-related operations are finished,
before re-polling (otherwise, during a `poll` call, the client vector could be modified and/or
resized, resulting in an invalid array of `struct pollfd`s).

Maintaining synchronized access to the client vector is described in a little more detail in the
comments for `poll_connections`.

### Structures

#### `station_t`

```c
typedef struct {
  sync_list_t client_list; // list to store clients connected to this station
  uint16_t station_number; // unique number for a station
  char *song_name;         // name of a song
  FILE *song_file;         // file to read the song
  char buf[CHUNK_SIZE];    // buffer to store processed song chunks
  pthread_t streamer;      // streamer thread
  int ipv4_stream_fd;      // IPv4 streaming socket
  int ipv6_stream_fd;      // IPv6 streaming socket
} station_t;
```

A `station_t` represents all operations of a station. Each station maintains a synchronized linked
list of clients in `client_list`; synchronization is maintained by wrapping locks around the
provided `list_t` macro implementation. The station's number and song name are stored as well, with
a corresponding `FILE*` to represent the song on disk.

Each station has a corresponding `streamer` thread responsible for broadcasting song data to
listening clients. Maintaining a `16Kbps` streaming rate is done as follows:

- Chunks of size `1024` bytes are read into `buf`. If the file reaches EOF before `1024` bytes are
  read, we `fseek` to the beginning of the file, and resume reading from the very beginning; an
  `ANNOUNCE` message is sent to all connected clients.
- Once a chunk is read, the station iterates through every client within the list, sending a UDP
  packet to the client.
- We have now sent `1/16` of the chunks necessary in a second to maintain `16Kbps`. The default
  sleep time is `0.0625s`, or `1/16` of a second; but this is assuming the prior two steps don't
  any time. In order to account for the processing time, we record the start and end times of
  reading then broadcasting to all clients, then subtract that computation time from `0.0625s`.

I originally stored two UDP sockets, one for IPv4 and one for IPv6 sockets; however, after the
announcement of requiring only IPv4, the IPv6 socket is no longer necessary. I currently don't have
time to remove it.

#### `client_vector_t`

```c
typedef struct {
  client_connection_t **conns; // array of connections
  struct pollfd *pfds;         // array of struct pollfds
  size_t size;                 // current size of a vector array
  size_t max;                  // current max size of a vector array
  int listener;                // listener socket
} client_vector_t;
```

A `client_vector_t` stores an array of clients, represented as two arrays of `client_connection_t`s
and `struct pollfd`s respectively. This separation, although highly unsightly, is necessary for
`poll` to work properly: since `poll` requires an array of `struct pollfd`s, we must maintain two
separate arrays. The other fields are necessary for implementing vector capabilities.

A client connection is represented as follows:

```c
typedef struct {
  list_link_t link;                 // for the doubly linked lists
  int client_fd;                    // TCP connection socket
  struct sockaddr_storage tcp_addr; // TCP address
  struct sockaddr_storage udp_addr; // UDP address
  socklen_t addr_len;  // address length; only difference is type + port
  int current_station; // currently connected station
} client_connection_t;
```

A client has both TCP and UDP addresses to represent the control and listener clients respectively;
the `link` is used to insert into linked lists.

## Snowcast Control

The snowcast control first attempts to connect to the server, then sends and waits for the server to
reply correctly. Then, the control client repeatedly polls both `stdin` and the `server_fd` for
input, spawning a thread for each request.

The snowcast control has one control structure:

```c
typedef struct {
  pthread_mutex_t control_mtx; // synchronize poll calls
  pthread_cond_t control_cond; // synchronize poll calls
  int num_events;              // record num pending
  int stopped;                 // record if client should stop
  int pending;                 // record if we're waiting for an Announce
  struct pollfd pfds[2];       // poll for `stdin` and `server_fd`
} snowcast_control_t;
```

The synchronization primitives and `num_events` are used for the same purposes as the server control
structure in `snowcast_server.c`, to ensure synchronization while `poll`ing.

## Snowcast Listener

idk man just look at it it's like 10 lines

# Bugs

Alas, some bugs still exist in the implementation.

Perhaps the biggest is a potential data race within the `for` loop for processing `poll` responses;
no mutex is maintained while we iterate through the indices within the client vector, so when we
access the `pfds` on line 561 in `snowcast_server.c`, there is a chance that the specific entry is
removed as we access, causing a segfault. I've run the testing script for `20` iterations, spawning
`500` clients that repeatedly swap servers, and I haven't managed to reproduce this segfault; but I
believe there's still a synchronization issue.

Shutting down the server operates "cleanly" in most cases, including when a client makes an invalid
call, in that all resources should be cleaned up properly and the server will exit. However, when I
compile the server with the thread sanitizer enabled, I receive multiple warnings about potential
double-unlocks or data races. While I don't believe this'd be a major issue---in that it only
happens on cleanup, and it appears that cleanup exits finally---it might be a point of concern. Not
sure.

When I broadcast `ANNOUNCE` messages to clients after a song repeats, I don't robustly error check
that the messages are sent; it might cause the streaming thread to crash, but I haven't tested it
yet.
