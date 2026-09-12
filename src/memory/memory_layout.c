#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int global_initialized = 42;
int global_uninitialized;

int main(void)
{
    static int static_initialized = 100;
    int stack_value = 7;

    const char *literal = "hello, memory";
    int *heap_value = malloc(sizeof(*heap_value));

    if (heap_value == NULL) {
        perror("malloc");
        return EXIT_FAILURE;
    }

    *heap_value = 123;

    printf("PID                  : %ld\n", (long)getpid());
    printf("global initialized   : %p\n", (void *)&global_initialized);
    printf("global uninitialized : %p\n", (void *)&global_uninitialized);
    printf("static initialized   : %p\n", (void *)&static_initialized);
    printf("string literal       : %p\n", (const void *)literal);
    printf("heap                  : %p\n", (void *)heap_value);
    printf("stack                 : %p\n", (void *)&stack_value);

    puts("");
    puts("Press Enter to exit...");
    getchar();

    free(heap_value);

    return EXIT_SUCCESS;
}
