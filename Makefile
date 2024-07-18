CC = gcc
CFLAGS = -Iinclude -Wall -Wextra -O2

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)

all: my_raft_app

my_raft_app: $(OBJ)
	$(CC) -o $@ $^

clean:
	rm -f $(OBJ) my_raft_app
