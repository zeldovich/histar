#include <inc/syscall.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/lib.h>

static void
gate_entry(void *arg, struct cobj_ref *msg)
{
    cprintf("server: gate_entry: %s\n", (char*) arg);
    cprintf("server: gate_entry: message %lx %lx\n",
	    msg->container, msg->object);

    msg->container = 0xc0ffee;
    msg->object = 0xdeadbeef;
}

static struct u_gate_entry ug;

int
main(int ac, char **av)
{
    cprintf("server process starting.\n");

    int rc = 1;		// abuse the root container

    char *msg = "Hello world.";
    int r = gate_create(&ug, rc, &gate_entry, msg);
    if (r < 0)
	panic("gate_create: %s", e2s(r));
}
