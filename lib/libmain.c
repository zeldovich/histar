#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/lib.h>

extern int main(int argc, char **argv);

uint64_t start_arg0;
uint64_t start_arg1;
start_env_t *start_env;

void
libmain(uint64_t arg0, uint64_t arg1)
{
    int argc = 0;
    char **argv = 0;

    start_arg0 = arg0;
    start_arg1 = arg1;

    if (start_arg1 == 0) {
	start_env = (start_env_t *) start_arg0;

	// set up argc, argv
    }

    main(argc, argv);
    sys_thread_halt();

    panic("libmain: still alive after sys_halt");
}
