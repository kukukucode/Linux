#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    printf("before exec: PID = %ld\n", (long)getpid());
    fflush(stdout);

    execl(
        "/bin/echo",
        "echo",
        "hello from exec",
        (char *)NULL
    );

    fprintf(stderr, "exec failed: %s\n", strerror(errno));

    return EXIT_FAILURE;
}
