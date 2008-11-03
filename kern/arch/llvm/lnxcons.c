#define _GNU_SOURCE 1

#include <kern/console.h>
#include <kern/arch.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>

static void
lnx_cons_putc(void *arg, int c, cons_source src)
{
    putc(c, stdout);
}

static int
lnx_cons_getc(void *arg)
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

void
lnx_cons_init(void)
{
    static struct cons_device cd = {
	.cd_pollin = &lnx_cons_getc,
	.cd_output = &lnx_cons_putc,
    };

    cons_register(&cd);
}
