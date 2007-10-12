#include <kern/lib.h>

int raise(int sig);

int __attribute__((noreturn))
raise(int sig)
{
    panic("raise %d", sig);
}

void
abort(void)
{
    for (;;)
	;
}
