#include <pthread.h>
#include <stdio.h>
#include <string.h>

static int shared_value = 10;

static void *worker(void *argument)
{
    (void)argument;

    int worker_local = 200;

    printf("worker: shared_value=%d address=%p\n",
           shared_value, (void *)&shared_value);

    printf("worker: local=%d address=%p\n",
           worker_local, (void *)&worker_local);

    shared_value = 42;

    return NULL;
}

int main(void)
{
    pthread_t thread;
    int main_local = 100;

    printf("main:   shared_value=%d address=%p\n",
           shared_value, (void *)&shared_value);

    printf("main:   local=%d address=%p\n",
           main_local, (void *)&main_local);

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

    printf("main:   after join shared_value=%d\n", shared_value);

    return 0;
}
