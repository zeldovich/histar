#include <inc/syscall.h>
#include <inc/setjmp.h>

#include <stdio.h>

int
main(int ac, char **av)
{
    struct jos_jmp_buf jb;

    int r = jos_setjmp(&jb);
    printf("jmptest: setjmp(): %d\n", r);

    if (r == 0) {
	printf("jmptest: longjmp'ing back\n");
	jos_longjmp(&jb, 0);
    }

    if (r == 1) {
	printf("jmptest: longjmp'ing back again\n");
	jos_longjmp(&jb, 2);
    }

    return 0;
}
