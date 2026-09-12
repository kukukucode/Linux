#include <stdlib.h>

int main(void)
{
    int *owner = malloc(sizeof(*owner));

    if (owner == NULL) {
        return EXIT_FAILURE;
    }

    *owner = 42;

    free(owner);

    /* Intentional bug for sanitizer experiment. */
    free(owner);

    return EXIT_SUCCESS;
}
