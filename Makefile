CC = gcc
CFLAGS = -Iinclude -Wall -O2
LDFLAGS = -lpthread

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)

all: raft_node

raft_node: $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) raft_node