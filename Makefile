CC ?= cc
CPPFLAGS ?= -D_POSIX_C_SOURCE=200809L
CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -Werror -g3 -O0
BUILD_DIR ?= build
TARGET := $(BUILD_DIR)/hello

.PHONY: all clean test

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $@

$(TARGET): src/hello.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@

test: all
	@output=`$(TARGET)`; test "$$output" = "Linux systems project is ready."
	@echo "All checks passed."

clean:
	rm -rf $(BUILD_DIR)
