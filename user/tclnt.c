#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/lib.h>
#include <inc/gate.h>

int
main(int ac, char **av)
{
    cprintf("client process starting.\n");

    // create a private handle for ourselves
    int64_t client_handle = sys_handle_create();
    if (client_handle < 0)
	panic("sys_handle_create: %s", e2s(client_handle));

    uint64_t myct = start_env->container;

    int64_t gate_id = container_find(start_env->root_container,
				     kobj_gate, "tserv");
    if (gate_id < 0)
	panic("finding tserv: %s", e2s(gate_id));

    struct cobj_ref gate = COBJ(start_env->root_container, gate_id);

    uint64_t a = 20;
    uint64_t b = 30;
    int round = 0;

    for (int i = 0; i < 10; i++) {
	struct ulabel *l = label_get_current();
	printf("tclnt: label %s\n", label_to_string(l));
	label_free(l);

	struct cobj_ref arg = COBJ(a, b);
	int r = gate_call(myct, gate, &arg);
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
