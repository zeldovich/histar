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

    //struct ulabel *l = label_get_current();
    //printf("tserv: label %s\n", label_to_string(l));
    //label_free(l);

    msg->container = x1 + x2;
    msg->object = 0;
}

static struct u_gate_entry ug;

int
main(int ac, char **av)
{
    cprintf("server process starting.\n");

    struct ulabel *l = label_get_current();
    assert(l);
    segment_default_label(l);

    char *msg = "Hello world.";
    int r = gate_create(&ug, start_env->root_container,
			&ts_gate_entry, msg, "tserv");
    if (r < 0)
	panic("gate_create: %s", e2s(r));
}
