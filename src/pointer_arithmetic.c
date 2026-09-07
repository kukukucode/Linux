#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    int values[] = {10, 20, 30, 40};

    int *p = values;

    printf("sizeof(int): %zu\n\n", sizeof(int));

    printf("p     : %p -> %d\n", (void *)p, *p);
    printf("p + 1 : %p -> %d\n", (void *)(p + 1), *(p + 1));
    printf("p + 2 : %p -> %d\n", (void *)(p + 2), *(p + 2));
    printf("p + 3 : %p -> %d\n", (void *)(p + 3), *(p + 3));

    return EXIT_SUCCESS;
}
