#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

struct LayoutExample {
    char a;
    int b;
    char c;
    double d;
};

int main(void)
{
    struct LayoutExample value = {
        .a = 'A',
        .b = 42,
        .c = 'C',
        .d = 3.14,
    };

    printf("sizeof(struct LayoutExample): %zu\n",
           sizeof(struct LayoutExample));

    printf("alignment                   : %zu\n",
           _Alignof(struct LayoutExample));

    printf("\nmember offsets\n");
    printf("a: %zu\n", offsetof(struct LayoutExample, a));
    printf("b: %zu\n", offsetof(struct LayoutExample, b));
    printf("c: %zu\n", offsetof(struct LayoutExample, c));
    printf("d: %zu\n", offsetof(struct LayoutExample, d));

    printf("\naddresses\n");
    printf("&value   : %p\n", (void *)&value);
    printf("&value.a : %p\n", (void *)&value.a);
    printf("&value.b : %p\n", (void *)&value.b);
    printf("&value.c : %p\n", (void *)&value.c);
    printf("&value.d : %p\n", (void *)&value.d);

    return EXIT_SUCCESS;
}
