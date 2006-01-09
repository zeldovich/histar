#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/lib.h>
#include <inc/gate.h>

int
main(int ac, char **av)
{
    cprintf("client process starting.\n");

    uint64_t rc = 1;		// abuse the root container
    uint64_t myct = start_env->container;

    int64_t rslots = sys_container_nslots(rc);
    if (rslots < 0)
	panic("sys_container_nslots: %s", e2s(rslots));

    int64_t id, i;
    for (i = 0; i < rslots; i++) {
	id = sys_container_get_slot_id(rc, i);
	if (id < 0)
	    continue;

	kobject_type_t type = sys_obj_get_type(COBJ(rc, id));
	if (type != kobj_gate)
	    continue;

	char name[KOBJ_NAME_LEN];
	int r = sys_obj_get_name(COBJ(rc, id), &name[0]);
	if (r < 0)
	    continue;

	if (!strcmp(&name[0], "tserv"))
	    break;
    }

    if (i == rslots)
	panic("cannot find any gates in root container %d", rc);

    uint64_t a = 20;
    uint64_t b = 30;
    int round = 0;

    for (;;) {
	struct cobj_ref arg = COBJ(a, b);
	int r = gate_call(myct, COBJ(rc, id), &arg);
	if (r < 0)
	    panic("gate_call: %s\n", e2s(r));

	uint64_t sum = arg.container;
	if (a + b != sum)
	    cprintf("incorrect result: %ld + %ld = %ld\n", a, b, sum);

	round++;
	if ((round % 20) == 0)
	    cprintf("tclnt %ld: did %d rounds\n", thread_id(), round);
    }
}
