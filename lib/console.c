#include <inc/syscall.h>
#include <inc/lib.h>

int
iscons(int fd)
{
    return 1;
}

int
getchar()
{
    return sys_cgetc();
}

int
putchar(int c)
{
    char b[2];
    b[0] = c;
    b[1] = '\0';
    sys_cputs(b);
    return 0;
}
