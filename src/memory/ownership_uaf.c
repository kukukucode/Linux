#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    int *owner = malloc(sizeof(*owner));

    if (owner == NULL) {
        perror("malloc");
        return EXIT_FAILURE;
    }

    *owner = 42;

    int *borrowed = owner;

    printf("owner address    : %p\n", (void *)owner);
    printf("borrowed address : %p\n", (void *)borrowed);

    free(owner);
    owner = NULL;

    printf("dangling value: %d\n", *borrowed);

    return EXIT_SUCCESS;
}
