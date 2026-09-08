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
        "/this/program/does/not/exist",
        "does-not-exist",
        (char *)NULL
    );

    fprintf(
        stderr,
        "exec failed: errno = %d, %s\n",
        errno,
        strerror(errno)
    );

    return EXIT_FAILURE;
}
