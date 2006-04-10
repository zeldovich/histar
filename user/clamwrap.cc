extern "C" {
#include <inc/syscall.h>

#include <stdio.h>
}

#include <inc/cpplabel.hh>
#include <inc/error.hh>

int
main(int ac, char **av)
try
{

} catch (std::exception &e) {
    printf("clamwrap: %s\n", e.what());
}
