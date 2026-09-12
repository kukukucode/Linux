CC ?= cc
CPPFLAGS ?= -D_POSIX_C_SOURCE=200809L
CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -Werror -g3 -O0

BUILD_DIR := build

# Intentionally broken programs are excluded from normal CI.
EXCLUDED_SOURCES := \
	src/ownership_uaf.c \
	src/ownership_double_free.c

SOURCES := $(filter-out $(EXCLUDED_SOURCES),$(wildcard src/*.c))
TARGETS := $(patsubst src/%.c,$(BUILD_DIR)/%,$(SOURCES))

PROCESS_SOURCES := $(wildcard src/process/*.c)
PROCESS_TARGETS := $(patsubst src/process/%.c,$(BUILD_DIR)/%,$(PROCESS_SOURCES))
TARGETS += $(PROCESS_TARGETS)

MINI_SHELL_SOURCES := \
	src/mini_shell/main.c \
	src/mini_shell/jobs.c \
	src/mini_shell/exec.c \
	src/mini_shell/parser.c
MINI_SHELL_HEADERS := src/mini_shell/mini_shell.h
TARGETS += $(BUILD_DIR)/mini_shell

.PHONY: all clean test

all: $(TARGETS)

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/%: src/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@

$(PROCESS_TARGETS): $(BUILD_DIR)/%: src/process/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@

$(BUILD_DIR)/mini_shell: $(MINI_SHELL_SOURCES) $(MINI_SHELL_HEADERS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(MINI_SHELL_SOURCES) -o $@

test: all
	@output=`$(BUILD_DIR)/hello`; \
		test "$$output" = "Linux systems project is ready."

	@$(BUILD_DIR)/ownership >/dev/null

	@output=`$(BUILD_DIR)/exec_basic`; \
		printf '%s\n' "$$output" | grep -q "hello from exec"

	@output=`$(BUILD_DIR)/pipe_basic`; \
		printf '%s\n' "$$output" | grep -q "hello through pipe"

	@output=`$(BUILD_DIR)/pipe_fork`; \
		printf '%s\n' "$$output" | grep -q "message from child"

	@output=`$(BUILD_DIR)/dup2_basic 2>&1`; \
		printf '%s\n' "$$output" | grep -q "hello through redirected stdout"

	@output=`$(BUILD_DIR)/pipe_exec`; \
		printf '%s\n' "$$output" | grep -q "hello from child through pipe"

	@output=`$(BUILD_DIR)/pipeline_two`; \
		printf '%s\n' "$$output" | grep -q '^6$$'

	@python3 tests/test_mini_shell.py

	@echo "All checks passed."

clean:
	rm -rf $(BUILD_DIR)
