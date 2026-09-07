CC ?= cc
CPPFLAGS ?= -D_POSIX_C_SOURCE=200809L
CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -Werror -g3 -O0

BUILD_DIR ?= build

HELLO_TARGET := $(BUILD_DIR)/hello
MEMORY_TARGET := $(BUILD_DIR)/memory_layout

TARGETS := $(HELLO_TARGET) $(MEMORY_TARGET)

.PHONY: all clean test

all: $(TARGETS)

$(BUILD_DIR):
	mkdir -p $@

$(HELLO_TARGET): src/hello.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@

$(MEMORY_TARGET): src/memory_layout.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@

test: all
	@output=`$(HELLO_TARGET)`; test "$$output" = "Linux systems project is ready."
	@echo "All checks passed."

clean:
	rm -rf $(BUILD_DIR)