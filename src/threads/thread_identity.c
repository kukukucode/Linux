#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void *worker(void *argument)
{
    (void)argument;

    printf("worker: pid=%ld\n", (long)getpid());

    sleep(15);

    return NULL;
}

int main(void)
{
    pthread_t thread;

    printf("main:   pid=%ld\n", (long)getpid());

    int error = pthread_create(&thread, NULL, worker, NULL);

    if (error != 0) {
        fprintf(stderr, "pthread_create failed: %s\n", strerror(error));
        return 1;
    }

    error = pthread_join(thread, NULL);

    if (error != 0) {
        fprintf(stderr, "pthread_join failed: %s\n", strerror(error));
        return 1;
    }

    return 0;
}
