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

    msg->container = x1 + x2;
    msg->object = 0;
}

static struct u_gate_entry ug;

int
main(int ac, char **av)
{
    cprintf("server process starting.\n");

    char *msg = "Hello world.";
    int r = gate_create(&ug, start_env->root_container,
			&ts_gate_entry, msg, "tserv");
    if (r < 0)
	panic("gate_create: %s", e2s(r));
}
