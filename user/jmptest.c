#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/setjmp.h>

int
main(int ac, char **av)
{
    struct jmp_buf jb;

    int r = setjmp(&jb);
    printf("jmptest: setjmp(): %d\n", r);

    if (r == 0) {
	printf("jmptest: longjmp'ing back\n");
	longjmp(&jb, 7);
    }

    return 0;
}
