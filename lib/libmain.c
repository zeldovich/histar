#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/lib.h>

extern int main(int argc, char **argv);

uint64_t start_arg;
uint64_t start_arg1;

void
libmain(uint64_t arg0, uint64_t arg1)
{
    start_arg = arg0;
    start_arg1 = arg1;

    main(0, 0);
    sys_thread_halt();

    panic("libmain: still alive after sys_halt");
}
