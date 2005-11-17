#include <inc/syscall.h>
#include <inc/stdio.h>

int
main(int ac, char **av)
{
    sys_cputs("Hello world.\n");
    cprintf("<%x>\n", 0xdeadbeef);

    return 0;
}
