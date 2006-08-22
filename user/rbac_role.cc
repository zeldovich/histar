extern "C" {
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/syscall.h>
#include <inc/gateparam.h>
#include <inc/syscall.h>
#include <inc/debug.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
}

#include <inc/rbac.hh>
#include <inc/spawn.hh>
#include <inc/error.hh>
#include <inc/labelutil.hh>
#include <inc/gatesrv.hh>
#include <inc/scopeguard.hh>

static const char dbg_label = 0;

static uint64_t grant_role;
static uint64_t taint_role;
static char open_mode = 0;

static const uint32_t trans_id_count = 16;
static uint64_t trans_id[trans_id_count];

static void __attribute__((noreturn))
acquire_gate(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    uint64_t *taint = (uint64_t *)parm->param_buf;
    label *dr = new label(0);
    dr->set(taint_role, 3);
    *taint = taint_role;

    label *ds = new label(3);
    ds->set(grant_role, LB_LEVEL_STAR);
    
    debug_print(dbg_label, "dr %s", dr->to_string());
    debug_print(dbg_label, "ds %s", ds->to_string());

    gr->ret(0, ds, dr);
}

static void __attribute__((noreturn))
trans_gate(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    struct gate_call_data back;
    gate_call_data_copy(&back, parm);
    struct cobj_ref target = parm->param_obj;
    char ok = 0;
    if (!open_mode) {
	for (uint32_t i = 0; i < trans_id_count; i++) {
	    if (trans_id[i] == target.object)
		ok = 1;
	}
    }
    else 
	ok = 1;

    if (!ok) 
	cprintf("trans_gate: error: trans not known\n");
    else
	rbac::gate_send(target, 0, 0);
    
    gate_call_data_copy(parm, &back);
    gr->ret(0,0,0);
}


static void __attribute__((noreturn))
admin_gate(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    uint64_t gate_id = parm->param_obj.object;
    for (uint32_t i = 0; i < trans_id_count; i++) {
	if (trans_id[i] == 0) {
	    trans_id[i] = gate_id;
	    gr->ret(0, 0, 0);
	}
    }
    cprintf("admin_gate: error: no space for trans\n");
    gr->ret(0, 0, 0);
}
int 
main (int ac, char **av)
{
    if (ac < 4) {
	cprintf("usage: %s name grant-category container\n", av[0]);
	return -1;
    }

    memset(trans_id, 0, sizeof(trans_id));
    uint64_t grant_root = atol(av[2]);
        
    try {
	char *name = av[1];
	uint64_t root_ct = atol(av[3]);

	grant_role = handle_alloc();
	taint_role = handle_alloc();
	
	uint64_t role_ct;
	label l(1);
	l.set(start_env->process_grant, 0);
	error_check(role_ct = sys_container_alloc(root_ct, l.to_ulabel(), name, 
						  0, CT_QUOTA_INF));
	label th_l;
	thread_cur_label(&th_l);

	label acquire_cl(2);
	acquire_cl.set(taint_role, 3);
	gate_create(role_ct, "acquire", &th_l, &acquire_cl, &acquire_gate, 0);
	
	label trans_cl(2);
	trans_cl.set(grant_role, 0);
	trans_cl.set(taint_role, 3);
	gate_create(role_ct, "trans", &th_l, &trans_cl, &trans_gate, 0);

	label admin_cl(2);
	admin_cl.set(grant_root, 0);
	gate_create(role_ct, "admin", &th_l, &admin_cl, &admin_gate, 0);
	
	debug_print(dbg_label, "label %s", th_l.to_string());
	debug_print(dbg_label, "acquire cl %s", acquire_cl.to_string());
	debug_print(dbg_label, "trans cl %s", trans_cl.to_string());

	thread_halt();
    } catch (basic_exception e) {
	cprintf("main: error: %s\n", e.what());
	return -1;
    }
    return 0;
}
