extern "C" {
#include <inc/lib.h>
#include <inc/stdio.h>
#include <stdlib.h>
}

#include <inc/labelutil.hh>
#include <inc/gatesrv.hh>

static void __attribute__((noreturn))
trans_gate(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    gr->ret(0,0,0);
}


int
main (int ac, char **av)
{

    if (ac < 4) {
	cprintf("usage: %s name grant-category container\n", av[0]);
	return -1;
    }
    
    char *name = av[1];
    uint64_t grant_role = atol(av[2]);
    uint64_t root_ct = atol(av[3]);

    label l, c;
    thread_cur_label(&l);
    thread_cur_clearance(&c);
    c.set(grant_role, 0);

    gate_create(root_ct, name, &l, &c, &trans_gate, 0);

    thread_halt();
    return 0;
}
