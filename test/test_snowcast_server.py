#!/usr/bin/env python3

import os
import pty
import random
import re
import select
import socket
import subprocess
import sys
import time
from collections import Counter
from os.path import join
from pathlib import Path
from typing import Dict, List

SNOWCAST_PATH = Path(__file__).resolve().parents[1]
TEST = Path(__file__).resolve().parents[0]
SCRIPTS = join(TEST, "scripts")
REFERENCE = join(SNOWCAST_PATH, "reference")
MP3 = join(SNOWCAST_PATH, "mp3")

STATIONS = [
    "/dev/urandom",
    join(SCRIPTS, "test.txt"),
    join(MP3, "Beethoven-SymphonyNo5.mp3"),
    join(MP3, "DukeEllington-Caravan.mp3"),
    join(MP3, "VanillaIce-IceIceBaby.mp3"),
    join(MP3, "ManchurianCandidates-TwentyEightHouse.mp3"),
    join(MP3, "FX-Impact193.mp3"),
]
NUM_STATIONS = len(STATIONS)

SERVER = join(SNOWCAST_PATH, "snowcast_server")
REFERENCE_CONTROL = join(REFERENCE, "snowcast_control")
REFERENCE_LISTENER = join(REFERENCE, "snowcast_listener")

#### HELPER FUNCTIONS
def compare(s, t):
    return Counter(s) == Counter(t)


def read_from_stdout(fd: int, timeout=0.5):
    while True:
        ready, _, _ = select.select([fd], [], [], timeout)
        if ready:
            data = os.read(fd, 1024)
            if not data:
                break
            print(data.decode("utf-8").rstrip(), end="")
        else:  # on timeout, quit
            break
    print()


def parse_stations(stations_file: str) -> Dict[str, List[str]]:
    stations = dict()
    with open(stations_file, "r", encoding="utf-8") as f:
        prev_station = 0
        for line in f:
            # if newline, continue
            if len(line) == 0:
                continue
            # if line is station, parse it
            if "station" in line.lower():
                # Get station number
                m = re.search(r"\d", line)
                if m:
                    station_number = int(m.group(0))
                    prev_station = station_number
                else:
                    raise Exception("Station malformatted; needs number")

                # Get station song
                song = line.split('"')[1::2][0]
                # make station
                stations[station_number] = Station(station_number, song, [])
            # otherwise, is a client
            else:
                # find start of client
                start = line.index("-") + 1
                stations[prev_station].add_client(line[start:].strip())

    return stations


class Station:
    def __init__(self, number: int, song: str, clients: List[str]):
        self.number: int = number
        self.song: str = song
        self.clients: List[str] = clients

    def add_client(self, client: str):
        self.clients.append(client)

    def has_client(self, client: str) -> bool:
        return client in self.clients

    def has_clients(self, clients: List[str]) -> bool:
        return compare(self.clients, clients)

    def remove_client(self, client: str):
        if self.has_client(client):
            print(f"Client {client} not in station {self.number}.")
        else:
            self.clients.remove(client)

    def __repr__(self):
        clients = ""
        for client in self.clients:
            clients += f"\t - {client}\n"
        return f'Station {self.number} ["{self.song}"] clients: \n{clients}'


class Server:
    def __init__(self):
        self.port = self.__get_open_port()
        # open with specified servers
        args = [SERVER] + [str(self.port)] + STATIONS
        # helps with pseudo-terminals?
        master, slave = pty.openpty()
        self.process = subprocess.Popen(
            args,
            stdin=subprocess.PIPE,
            stdout=slave,
            stderr=slave,
        )
        self.sfd = master
        read_from_stdout(self.sfd)

    def __get_open_port(self):
        # open IPv4 TCP socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        # bind to any open port
        sock.bind(("", 0))
        # get socket name (<address>, <port>)
        port = sock.getsockname()[1]
        # close when done
        sock.close()
        return port

    def is_dead(self):
        return self.process.poll() is not None

    def get_station_clients(self, result_file=join(TEST, "stations.txt")):
        if os.path.isfile(result_file):
            os.remove(result_file)

        command = "p %s\n" % result_file
        self.process.stdin.write(command.encode("utf-8"))
        self.process.stdin.flush()

        if self.is_dead():
            raise Exception("Server terminated while saving database")
        # wait for up to 5s for the file to be written
        for _ in range(5):
            if os.path.isfile(result_file):
                break
            time.sleep(1)
        # if not created,
        if not os.path.isfile(result_file):
            raise Exception("Server failed to save database after 30 seconds")

        stations = parse_stations(result_file)
        return stations

    def __cleanup(self):
        self.process.stdin.close()
        os.close(self.sfd)

    def quit_server(self):
        self.process.stdin.write(b"q\n")
        self.process.stdin.flush()
        read_from_stdout(self.sfd)
        self.__cleanup()


class Client:
    def __init__(self, server_port: int, listener_port: int):
        self.server_port = server_port
        self.listener_port = listener_port
        self.out = open(os.devnull, "w")
        self.station = -1
        self.process = subprocess.Popen(
            [REFERENCE_CONTROL, "localhost", str(server_port), str(listener_port)],
            stdin=subprocess.PIPE,
            stdout=self.out,
            stderr=self.out,
        )

    def join_station(self, station: int):
        command = "%s\n" % str(station)
        try:
            self.process.stdin.write(command.encode("utf-8"))
            self.process.stdin.flush()
            self.station = station
        except Exception as e:
            self.quit()

    def is_dead(self):
        return self.process.poll() is not None

    def __repr__(self):
        return f"Client -> Server [localhost:{self.server_port}], Station [{self.station}], Listener: [{self.listener_port}]"

    def __cleanup(self):
        self.out.close()

    def quit(self, timeout=1):
        self.process.kill()
        self.__cleanup()


#  server = Server()
#  clients = list()
#  for i in range(50):
#  clients.append(Client(server.port, 10000 + i))
#  print(f"Created client {i}: {clients[i]}")

#  time.sleep(1)
#  read_from_stdout(server.sfd)

#  for _ in range(5):
#  for client in clients:
#  station = random.randint(0, NUM_STATIONS)
#  client.join_station(station)
#  time.sleep(0.5)

#  stations = server.get_station_clients()

#  for station in stations:
#  print(station)

#  for client in clients:
#  client.quit()

#  server.quit_server()


server_port = 9000
num_clients = 500
num_iters = 200

clients = list()

for i in range(num_clients):
    clients.append(Client(server_port, 10000 + i))
    print(f"Created client {i}: {clients[i]}")

for _ in range(num_iters):
    for i in range(num_clients):
        if clients[i].is_dead():
            clients[i] = Client(server_port, 10000 + i)
        print(f"Created client {i}: {clients[i]}")

    for _ in range(5):
        for i in range(len(clients)):
            curr_client = clients[i]
            if curr_client.is_dead():
                curr_client.quit()
                clients[i] = Client(server_port, curr_client.listener_port)
            clients[i].join_station(random.randint(0, 7))
        time.sleep(0.2)

    print("Listening...")
    time.sleep(1.5)

    for client in clients:
        # randonly kill a client
        if random.uniform(0, 1) < 0.25:
            client.quit()
    print("Randomly quit out.")
    time.sleep(1)
