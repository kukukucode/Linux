#include <stdio.h>
#include <stdlib.h>

static int *move_int(int **source)
{
    int *destination = *source;
    *source = NULL;

    return destination;
}

int main(void)
{
    int *owner_a = malloc(sizeof(*owner_a));

    if (owner_a == NULL) {
        perror("malloc");
        return EXIT_FAILURE;
    }

    *owner_a = 42;

    printf("before move\n");
    printf("owner_a: %p\n", (void *)owner_a);

    int *owner_b = move_int(&owner_a);

    printf("\nafter move\n");
    printf("owner_a: %p\n", (void *)owner_a);
    printf("owner_b: %p\n", (void *)owner_b);
    printf("value  : %d\n", *owner_b);

    free(owner_b);
    owner_b = NULL;

    return EXIT_SUCCESS;
}
