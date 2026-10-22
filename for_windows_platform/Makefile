CC := gcc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude
LDFLAGS ?= -static
LDLIBS := -lsodium -lws2_32

BUILD_DIR := build
TARGET := $(BUILD_DIR)/chat.exe
SOURCES := \
	src/main.c \
	src/cli_args.c \
	src/chat.c \
	src/network.c \
	src/crypto.c
OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
DEPFILES := $(OBJECTS:.o=.d)

.PHONY: all check-env clean

all: check-env $(TARGET)

check-env:
	@if [ "$$MSYSTEM" != "UCRT64" ]; then \
		echo "Run make from the MSYS2 UCRT64 terminal." >&2; \
		exit 1; \
	fi
	@if [ ! -f "$$MINGW_PREFIX/include/sodium.h" ]; then \
		echo "libsodium headers not found. Install mingw-w64-ucrt-x86_64-libsodium." >&2; \
		exit 1; \
	fi
	@if [ ! -f "$$MINGW_PREFIX/lib/libsodium.a" ]; then \
		echo "Static libsodium library not found. Reinstall mingw-w64-ucrt-x86_64-libsodium." >&2; \
		exit 1; \
	fi

$(TARGET): Makefile $(OBJECTS) | check-env
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPFILES)
