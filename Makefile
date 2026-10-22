CC = gcc

CFLAGS = -Wall -Wextra -Wpedantic -std=c2x -O2 -Iinclude
LDFLAGS = -pthread -lsodium

TARGET = chat

SRC = src/main.c src/cli_args.c src/chat.c src/network.c src/crypto.c

OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

TEST_TARGETS = \
	tests/test_network \
	tests/test_cli_args \
	tests/test_crypto \
	tests/test_chat \
	tests/test_main

tests/test_network: tests/test_network.c tests/test_support.h src/crypto.c
	$(CC) $(CFLAGS) tests/test_network.c src/crypto.c -o $@ $(LDFLAGS)

tests/test_cli_args: tests/test_cli_args.c tests/test_support.h src/cli_args.c
	$(CC) $(CFLAGS) tests/test_cli_args.c src/cli_args.c -o $@ $(LDFLAGS)

tests/test_crypto: tests/test_crypto.c tests/test_support.h src/crypto.c
	$(CC) $(CFLAGS) tests/test_crypto.c src/crypto.c -o $@ $(LDFLAGS)

tests/test_chat: tests/test_chat.c tests/test_support.h src/crypto.c
	$(CC) $(CFLAGS) tests/test_chat.c src/crypto.c -o $@ $(LDFLAGS)

tests/test_main: tests/test_main.c tests/test_support.h src/cli_args.c
	$(CC) $(CFLAGS) tests/test_main.c src/cli_args.c -o $@ $(LDFLAGS)

test: $(TEST_TARGETS)
	@set -e; passed=0; total=0; \
	for test in $(TEST_TARGETS); do \
		output=$$(./$$test 2>&1) || { printf '%s\n' "$$output"; exit 1; }; \
		printf '%s\n' "$$output"; \
		result=$$(printf '%s\n' "$$output" | sed -n 's/^TEST_RESULT \([0-9][0-9]*\)\/\([0-9][0-9]*\)$$/\1 \2/p'); \
		set -- $$result; \
		if [ "$$#" -ne 2 ]; then echo "Missing test result from $$test" >&2; exit 1; fi; \
		passed=$$((passed + $$1)); \
		total=$$((total + $$2)); \
	done; \
	printf '\n\nTotal: %s/%s tests passed\n' "$$passed" "$$total"

test-network: tests/test_network
	./tests/test_network

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET) $(TEST_TARGETS)

re: clean all

.PHONY: all clean re test test-network