#include <inc/syscall.h>
#include <inc/stdio.h>

int
main(int ac, char **av)
{
    cprintf("Hello world: 0x%x\n", 0xdeadbeef);

    return 0;
}
