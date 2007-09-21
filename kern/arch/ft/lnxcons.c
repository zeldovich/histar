#define _GNU_SOURCE 1

#include <dev/console.h>
#include <kern/arch.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>

struct Thread_list console_waiting;

void
cons_putc(int c)
{
    putc(c, stdout);
}

int
cons_getc(void)
{
    int flags = fcntl(0, F_GETFL);
    fcntl(0, F_SETFL, flags | O_NONBLOCK);

    char c = 0;
    int r = read(0, &c, 1);
    if (r < 0 && errno != EAGAIN)
	perror("cons_getc/read");

    fcntl(0, F_SETFL, flags);
    return c;
}

int
cons_probe(void)
{
    return 0;
}

void
cons_cursor(int line, int col)
{
    printf("cons_cursor(%d, %d)\n", line, col);
}
