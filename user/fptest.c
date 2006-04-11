#include <stdio.h>
#include <math.h>

enum { rounds = 1000000 };

int
main(int ac, char **av)
{
    double a, b, c, d;

    a = 1.0;
    b = 3.0;
    for (int i = 0; i < rounds; i++) {
	a += 100.0 * sin(a + b);
	a -= 100.0 * cos(a + b);
    }

    c = 1.0;
    d = 3.0;
    for (int i = 0; i < rounds; i++) {
	c += 100.0 * sin(c + d);
	c -= 100.0 * cos(c + d);
    }

    printf("a = %f\n", a);
    printf("c = %f\n", c);
}
