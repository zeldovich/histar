#include <kern/console.h>

static LIST_HEAD(cd_list, cons_device) cdevs;
struct Thread_list console_waiting;

enum { cons_bufsize = 512 };

static struct {
    uint8_t buf[cons_bufsize];
    uint32_t rpos;
    uint32_t wpos;
} cons_inq;

void
cons_putc(int c)
{
    struct cons_device *cd;
    LIST_FOREACH(cd, &cdevs, cd_link)
	if (cd->cd_output)
	    cd->cd_output(cd->cd_arg, c);
}

int
cons_getc(void)
{
    /* Check for input if interrupts are disabled */
    cons_probe();

    if (cons_inq.rpos != cons_inq.wpos) {
	int c = cons_inq.buf[cons_inq.rpos++];
	if (cons_inq.rpos == sizeof(cons_inq.buf))
	    cons_inq.rpos = 0;
	return c;
    }

    return 0;
}

int
cons_probe(void)
{
    struct cons_device *cd;
    LIST_FOREACH(cd, &cdevs, cd_link)
	if (cd->cd_pollin)
	    cons_intr(cd->cd_pollin, cd->cd_arg);

    return cons_inq.rpos != cons_inq.wpos;
}

void
cons_intr(int (*proc)(void*), void *arg)
{
    int c;

    while ((c = (*proc)(arg)) != -1) {
	if (c == 0)
	    continue;
	cons_inq.buf[cons_inq.wpos++] = c;
	if (cons_inq.wpos == sizeof(cons_inq.buf))
	    cons_inq.wpos = 0;
    }

    while (!LIST_EMPTY(&console_waiting)) {
	struct Thread *t = LIST_FIRST(&console_waiting);
	thread_set_runnable(t);
    }
}

void
cons_register(struct cons_device *cd)
{
    LIST_INSERT_HEAD(&cdevs, cd, cd_link);
}
