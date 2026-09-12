#include <pthread.h>
#include <stdio.h>
#include <string.h>

static void *worker(void *argument)
{
    (void)argument;

    printf("worker: hello from thread\n");

    return NULL;
}

int main(void)
{
    pthread_t thread;

    printf("main: before pthread_create\n");

    int error = pthread_create(&thread, NULL, worker, NULL);

    if (error != 0) {
        fprintf(stderr, "pthread_create failed: %s\n", strerror(error));
        return 1;
    }

    printf("main: after pthread_create\n");

    error = pthread_join(thread, NULL);

    if (error != 0) {
        fprintf(stderr, "pthread_join failed: %s\n", strerror(error));
        return 1;
    }

    printf("main: after pthread_join\n");

    return 0;
}
