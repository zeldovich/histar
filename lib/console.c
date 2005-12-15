#include <inc/syscall.h>
#include <inc/lib.h>

int
iscons(int fd)
{
    return 1;
}

int
getchar(void)
{
    return sys_cons_getc();
}

int
putchar(int c)
{
    sys_cons_puts((char*) &c, 1);
    return 0;
}
