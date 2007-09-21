#include <stdio.h>

static void
foo(int s)
{
    char a[s];
    int r = 0;

    for (int i = 0; i < s; i++)
	a[i] = i;
    for (int i = 0; i < s; i++)
	r += a[i];

    printf("foo(%d): %d\n", s, r);
}

int
main(int ac, char **av)
{
    int s = 4096;

    foo(s);
    foo(s * 2);
    foo(s * 4);
    foo(s * 8);
    foo(s * 16);
}
