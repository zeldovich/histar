#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/lib.h>

extern int main(int argc, const char **argv);

uint64_t start_arg0;
uint64_t start_arg1;
start_env_t *start_env;

#define MAXARGS	16

void
libmain(uint64_t arg0, uint64_t arg1)
{
    int argc = 0;
    const char *argv[MAXARGS];

    memset(&argv, 0, sizeof(argv));

    start_arg0 = arg0;
    start_arg1 = arg1;

    if (start_arg1 == 0) {
	start_env = (start_env_t *) start_arg0;

	const char *p = &start_env->args[0];
	for (int i = 0; i < MAXARGS; i++) {
	    size_t len = strlen(p);
	    if (len == 0)
		break;

	    argv[argc] = p;
	    p += len + 1;
	    argc++;
	}
    }

    main(argc, &argv[0]);

    close_all();
    sys_thread_halt();

    panic("libmain: still alive after sys_halt");
}
