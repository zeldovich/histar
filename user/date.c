#include <stdio.h>
#include <time.h>

int
main(int ac, char **av)
{
    time_t t = time(0);
    printf("%s", asctime(localtime(&t)));
}
