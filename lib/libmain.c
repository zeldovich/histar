#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/lib.h>

extern int main(int argc, char **argv);
uint64_t start_arg;

void
libmain(uint64_t arg)
{
    start_arg = arg;
    main(0, 0);
    sys_thread_halt();

    panic("libmain: still alive after sys_halt");
}
