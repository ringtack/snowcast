CC = gcc

# Source and Build directories
SRC = ./src
UTIL = $(SRC)/util
BUILD = .
OBJDIR = $(BUILD)/build

# Objects to compile
OBJS = $(patsubst $(UTIL)/%.c, $(OBJDIR)/%.o, $(wildcard $(UTIL)/*.c))
FILES = $(wildcard src/*.c src/*.h)
EXECS = snowcast_control snowcast_listener snowcast_server

# Include util folders!
FLAGS = -Wall -Wextra -Wno-sign-compare -pthread -ggdb3 -I$(UTIL) -fsanitize=address

# Pretty printing
TOILET = toilet -f term -F border:metal



.PHONY: all clean format

all: | PRINT_START $(OBJDIR) $(EXECS) PRINT_DONE

$(OBJDIR):
	@echo "$$($(TOILET) -F gay Build directory does not exist. Creating at \"$(OBJDIR)\"...)"
	mkdir -p $(OBJDIR)
	@echo "$$($(TOILET) -F gay Done!)"

$(OBJS): PRINT_OBJ | $(OBJDIR)

PRINT_START:
	@echo "$$($(TOILET) -f pagga BUILD)"

PRINT_OBJ:
	@echo "$$($(TOILET) Building object files...)"

PRINT_DONE:
	@echo
	@echo "$$($(TOILET) -f pagga USAGE)"
	@echo "Finished building. To use:"
	@echo "\t - ./snowcast_server <PORT> [FILE1 [FILE2 [...]]]"
	@echo "\t - ./snowcast_control <SERVERNAME> <SERVERPORT> <UDPPORT>"
	@echo "\t - ./snowcast_listener <UDPPORT>"


$(OBJDIR)/%.o: $(UTIL)/%.c $(UTIL)/%.h
	$(CC) $(FLAGS) -c $< -o $@

snowcast_control: $(OBJS) $(SRC)/snowcast_control.c
	@echo "$$($(TOILET) Building snowcast_control...)"
	$(CC) $(FLAGS) $^ -o $(BUILD)/$@

snowcast_listener: $(OBJS) $(SRC)/snowcast_listener.c
	@echo "$$($(TOILET) Building snowcast_listener...)"
	$(CC) $(FLAGS) $^ -o $(BUILD)/$@

snowcast_server: $(OBJS) $(SRC)/snowcast_server.c
	@echo "$$($(TOILET) Building snowcast_server...)"
	$(CC) $(FLAGS) $^ -o $(BUILD)/$@

clean:
	@echo "$$($(TOILET) -f pagga CLEAN)"
	@echo "$$($(TOILET) -F gay Removing build files and executables...)"
	rm -f snowcast_*
	rm -rf $(OBJDIR)
	@echo "$$($(TOILET) -F gay Done.)"
	@echo

format:
	clang-format -style=file -i $(FILES)
