CC = gcc

CFLAGS = -Wall -Wextra -Wpedantic -std=c2x -O3 -Iinclude
LDFLAGS = -pthread

TARGET = chat

SRC = src/main.c src/cli_args.c src/chat.c src/network.c

OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

re: clean all

.PHONY: all clean re