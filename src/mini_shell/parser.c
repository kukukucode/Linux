#include <stdio.h>
#include <string.h>

#include "mini_shell.h"

int parse_line(char *line, struct ParsedLine *parsed)
{
    memset(parsed, 0, sizeof(*parsed));

    parsed->command_count = 1;

    char *saveptr = NULL;

    char *token = strtok_r(line, " \t\n", &saveptr);

    while (token != NULL) {
        size_t current = parsed->command_count - 1;

        if (strcmp(token, "&") == 0) {
            if (parsed->background) {
                fprintf(stderr, "mini-shell: multiple & operators\n");
                return -1;
            }

            parsed->background = 1;

            token = strtok_r(NULL, " \t\n", &saveptr);

            if (token != NULL) {
                fprintf(stderr, "mini-shell: & must appear at the end\n");
                return -1;
            }

            break;
        }

        if (strcmp(token, "|") == 0) {
            if (parsed->argcs[current] == 0) {
                fprintf(stderr, "mini-shell: expected command before |\n");
                return -1;
            }

            if (parsed->command_count >= MAX_COMMANDS) {
                fprintf(stderr, "mini-shell: too many pipeline commands\n");
                return -1;
            }

            ++parsed->command_count;
        } else if (strcmp(token, "<") == 0) {
            if (parsed->input_path != NULL) {
                fprintf(stderr, "mini-shell: multiple input redirects\n");
                return -1;
            }

            token = strtok_r(NULL, " \t\n", &saveptr);

            if (token == NULL || strcmp(token, "<") == 0 ||
                strcmp(token, ">") == 0 || strcmp(token, "|") == 0) {
                fprintf(stderr, "mini-shell: expected filename after <\n");
                return -1;
            }

            parsed->input_path = token;
            parsed->input_command = current;
        } else if (strcmp(token, ">") == 0) {
            if (parsed->output_path != NULL) {
                fprintf(stderr, "mini-shell: multiple output redirects\n");
                return -1;
            }

            token = strtok_r(NULL, " \t\n", &saveptr);

            if (token == NULL || strcmp(token, "<") == 0 ||
                strcmp(token, ">") == 0 || strcmp(token, "|") == 0) {
                fprintf(stderr, "mini-shell: expected filename after >\n");
                return -1;
            }

            parsed->output_path = token;
            parsed->output_command = current;
        } else {
            if (parsed->argcs[current] >= MAX_ARGS - 1) {
                fprintf(stderr, "mini-shell: too many arguments\n");
                return -1;
            }

            parsed->commands[current][parsed->argcs[current]++] = token;
        }

        token = strtok_r(NULL, " \t\n", &saveptr);
    }

    /*
     * execvp() requires NULL-terminated argv arrays.
     */
    for (size_t i = 0; i < parsed->command_count; ++i) {
        parsed->commands[i][parsed->argcs[i]] = NULL;
    }

    return 0;
}
