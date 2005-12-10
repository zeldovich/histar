#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/lib.h>

int
main(int ac, char **av)
{
    cprintf("client process starting.\n");

    int rc = 1;		// abuse the root container
    int myct = start_arg;

    int rslots = sys_container_nslots(rc);
    if (rslots < 0)
	panic("sys_container_nslots: %s", e2s(rslots));

    int i;
    for (i = 0; i < rslots; i++) {
	kobject_type_t type = sys_obj_get_type(COBJ(rc, i));
	if (type == kobj_gate)
	    break;
    }

    if (i == rslots)
	panic("cannot find any gates in root container %d", rc);

    cprintf("client: about to call into gate\n");
    int r = gate_call(myct, COBJ(rc, i));
    if (r < 0)
	panic("gate_call: %s\n", e2s(r));

    cprintf("client: back from the gate call\n");
}
