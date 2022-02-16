# Snowcast

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
> rather arbitrarily. I will detail this slightly more in the section on stations.

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
