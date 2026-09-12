#include <pthread.h>
#include <stdio.h>
#include <string.h>

#define ITERATIONS 1000000

static long counter = 0;

static void *worker(void *argument)
{
    (void)argument;

    for (long i = 0; i < ITERATIONS; ++i) {
        ++counter;
    }

    return NULL;
}

int main(void)
{
    pthread_t first;
    pthread_t second;

    int error = pthread_create(&first, NULL, worker, NULL);

    if (error != 0) {
        fprintf(stderr, "pthread_create failed: %s\n", strerror(error));
        return 1;
    }

    error = pthread_create(&second, NULL, worker, NULL);

    if (error != 0) {
        fprintf(stderr, "pthread_create failed: %s\n", strerror(error));
        return 1;
    }

    error = pthread_join(first, NULL);

    if (error != 0) {
        fprintf(stderr, "pthread_join failed: %s\n", strerror(error));
        return 1;
    }

    error = pthread_join(second, NULL);

    if (error != 0) {
        fprintf(stderr, "pthread_join failed: %s\n", strerror(error));
        return 1;
    }

    printf("expected=%ld\n", 2L * ITERATIONS);
    printf("actual=%ld\n", counter);

    return 0;
}
