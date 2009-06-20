extern "C" {
#include <inc/stdio.h>
#include <inc/error.h>
#include <inc/error.hh>

#include <stdio.h>
#include <string.h>
}

int
main(int ac, char **av)
try
{
    printf("--> Firing exception now (if I die, it failed)\n");
    throw basic_exception("SUCCESS!");
    printf("--> FAILED\n");
    return -1;
} catch (std::exception &e) {
    printf("--> exception: %s\n", e.what());
    return 0;
}
