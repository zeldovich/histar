#include <inc/syscall.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/lib.h>
#include <inc/gate.h>

static void
ts_gate_entry(void *arg, struct cobj_ref *msg)
{
    uint64_t x1 = msg->container;
    uint64_t x2 = msg->object;
    uint64_t r = 0;
    for (uint64_t i = 0; i < x1; i++)
	r++;
    for (uint64_t i = 0; i < x2; i++)
	r++;

    msg->container = r;
    msg->object = 0;
}

static struct u_gate_entry ug;

int
main(int ac, char **av)
{
    cprintf("server process starting.\n");

    int rc = 1;		// abuse the root container

    char *msg = "Hello world.";
    int r = gate_create(&ug, rc, &ts_gate_entry, msg);
    if (r < 0)
	panic("gate_create: %s", e2s(r));
}
