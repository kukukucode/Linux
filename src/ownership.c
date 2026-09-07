#include <stdio.h>
#include <stdlib.h>

static void print_borrowed(const int *value)
{
    printf("borrowed value: %d\n", *value);
}

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

    print_borrowed(borrowed);

    free(owner);
    owner = NULL;

    return EXIT_SUCCESS;
}