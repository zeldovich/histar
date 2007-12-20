#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <inc/lib.h>

int
main(int ac, char **av)
{
    start_env->jos_trace_on = 1;
    execv(av[1], av + 1);
    perror("execv");
}
