CC = gcc
CFLAGS = -Iinclude -Wall -O2

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)

all: raft

raft: $(OBJ)
	$(CC) -o $@ $^

clean:
	rm -f $(OBJ) raft
