CC = gcc
FLAGS = -Wall -Wextra -pthread -g

SRC = src
UTIL = $(SRC)/util
OBJS = $(patsubst $(UTIL)/%.c, $(UTIL)/%.o, $(wildcard $(UTIL)/*.c))
FILES = $(wildcard src/*.c src/*.h)
EXECS = snowcast_control snowcast_listener snowcast_server

.PHONY: all clean format

all: $(EXECS)

$(UTIL)/%.o: $(UTIL)/%.c $(UTIL)/%.h
	$(CC) $(FLAGS) -c $< 

snowcast_control: $(OBJS) $(SRC)/snowcast_control.c
	$(CC) $(FLAGS) $^ -o $@

snowcast_listener: $(OBJS) $(SRC)/snowcast_listener.c
	$(CC) $(FLAGS) $^ -o $@

snowcast_server: $(OBJS) $(SRC)/snowcast_server.c
	$(CC) $(FLAGS) $^ -o $@

clean:
	rm -f snowcast_* $(UTIL)/*.o

format:
	clang-format -style=file -i $(FILES)
