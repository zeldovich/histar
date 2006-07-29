extern "C" {
#include <inc/gateparam.h>
#include <inc/syscall.h>
#include <inc/error.h>
#include <inc/stdio.h>
#include <inc/debug.h>

#include <inc/dis/share.h>

#include <string.h>

}

#include <inc/dis/globallabel.hh>
#include <inc/dis/shareutils.hh>

#include <inc/labelutil.hh>
#include <inc/gateclnt.hh>
#include <inc/gatesrv.hh>
#include <inc/error.hh>
#include <inc/scopeguard.hh>

static const char debug_user = 1;
static const char debug_client = 1;
static const char debug_admin = 1;

extern const char *__progname;

static struct {
    struct { 
	uint64_t id;
	cobj_ref gate;
    } servers[10];
    struct { 
	uint64_t id;
	cobj_ref gate;
    } clients[10];
} admin_info;

static uint64_t local_grants = 0;
static uint64_t principal_id = 0;

static void __attribute__((noreturn))
grant_cat(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    uint64_t local = (uint64_t) arg;
    label *dl = new label(3);
    dl->set(local, LB_LEVEL_STAR);
    gr->ret(0, dl, 0);        
}

///////
// user
///////

static void
user_add_local_cat(share_args *args)
{
    char buffer[32];
    uint64_t cat = args->add_local_cat.cat;
    label th_l, th_cl(2);
    thread_cur_label(&th_l);
    th_cl.set(start_env->process_grant, 0);

    cprintf("cat %ld th_l %s\n", cat, th_l.to_string());
    
    sprintf(buffer, "%ld", cat);
    gate_create(local_grants, buffer, &th_l, &th_cl, &grant_cat, (void *)cat);
    return;
}

static void
user_observe(share_args *args)
{
    cobj_ref gate = COBJ(0,0);
    
    for (int i = 0; i < 10; i++) {
	if (admin_info.servers[i].id == args->observe.id) {
	    gate = admin_info.servers[i].gate;
	    break;
	}
    }
    
    if (!gate.object)
	throw error(E_NOT_FOUND,"server %ld", args->observe.id);
    
    struct share_server_args sargs;
    sargs.op = share_server_read;
    memcpy(&sargs.resource, &args->observe.res, sizeof(sargs.resource));
    error_check(gate_send(gate, &sargs, sizeof(sargs), 0));
    
    args->ret = sargs.ret;
}

static void
user_modify(share_args *args)
{
    ;
}

static void __attribute__((noreturn))
user_gate(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    gate_call_data bck;
    gate_call_data_copy_all(&bck, parm);
    share_args *args = (share_args*) parm->param_buf;

    debug_print(debug_user, "%s: op %d", __progname, args->op);
    
    try {
	switch(args->op) {
	case share_observe:
	    user_observe(args);
	    break;
	case share_modify:
	    user_modify(args);
	    break;
	case share_add_local_cat:
	    user_add_local_cat(args);
	    break;
	default:
	    throw basic_exception("unknown op %d", args->op);
	}
    } catch (basic_exception e) {
	cprintf("user_gate: %s\n", e.what());
	args->ret = -1;
    }
    gate_call_data_copy_all(parm, &bck);
    gr->ret(0, 0, 0);    
}

///////
// client
///////

static int64_t
get_grant_gate(uint64_t cat)
{
    char buf[16];
    sprintf(buf, "%ld", cat);
    return container_find(local_grants, kobj_gate, buf);
}

static void 
get_global(uint64_t cat, global_cat *gcat)
{
    error_check(get_grant_gate(cat));
    gcat->k = principal_id;
    gcat->original = cat;
}

static void
client_grant(share_args *args, label *dl)
{
    struct ulabel *ul = label_alloc();
    error_check(sys_obj_get_label(args->grant.obj, ul));
    
    label l(1);
    l.copy_from(ul);
    global_label gl(&l, &get_global);
    
    for (uint32_t i = 0; i < ul->ul_nent; i++) {
	uint64_t cat = LB_HANDLE(ul->ul_ent[i]);
	uint64_t gt = get_grant_gate(cat);
	gate_send(COBJ(local_grants, gt), 0, 0, 0);
	dl->set(cat, LB_LEVEL_STAR);
    }
    args->ret = 0;
}

static void __attribute__((noreturn))
client_gate(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    gate_call_data bck;
    gate_call_data_copy(&bck, parm);
    share_args *args = (share_args*) parm->param_buf;

    debug_print(debug_client, "%s: op %d", __progname, args->op);
    
    label *dl = new label(3);
    try {
	switch(args->op) {
	case share_grant:
	    client_grant(args, dl);
	    break;
	default:
	    throw basic_exception("unknown op %d", args->op);
	}
    } catch (basic_exception e) {
	cprintf("client_gate: %s\n", e.what());
	args->ret = -1;
    }
    gate_call_data_copy(parm, &bck);
    gr->ret(0, dl, 0);    
}

///////
// admin
///////

static void
admin_add_server(share_args *args)
{

    for (int i = 0; i < 10; i++) {
	if (!admin_info.servers[i].id) {
	    admin_info.servers[i].id = args->add_principal.id;
	    admin_info.servers[i].gate = args->add_principal.gate;
	    args->ret = 0;
	    return ;
	}
    }
    throw basic_exception("admin_add_server: cannot add server");
}

static void
admin_add_client(share_args *args)
{
    for (int i = 0; i < 10; i++) {
	if (!admin_info.clients[i].id) {
	    admin_info.clients[i].id = args->add_principal.id;
	    admin_info.clients[i].gate = args->add_principal.gate;
	    args->ret = 0;
	    return;
	}
    }
    throw basic_exception("admin_add_client: cannot add client");
}


static void __attribute__((noreturn))
admin_gate(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    gate_call_data bck;
    gate_call_data_copy(&bck, parm);
    share_args *args = (share_args*) parm->param_buf;

    debug_print(debug_admin, "%s: op %d", __progname, args->op);
    
    try {
	switch (args->op) {
	case share_add_server:
	    admin_add_server(args);
	    break;
	case share_add_client:
	    admin_add_client(args);
	    break;
	default:
	    throw basic_exception("unknown op %d", args->op);
	}
    }
    catch (basic_exception e) {
	cprintf("admin_gate: %s\n", e.what());
	args->ret = -1;
    }

    gate_call_data_copy(parm, &bck);
    gr->ret(0, 0, 0);    
}

int
main (int ac, char **av)
{
    memset(&admin_info, 0, sizeof(admin_info));
    
    label l(1);
    l.set(start_env->process_taint, 3);
    int64_t ct;
    error_check(ct = sys_container_alloc(start_env->proc_container, 
					 l.to_ulabel(), "local grants", 
					 0, CT_QUOTA_INF));
    local_grants = ct;

    label th_l, th_cl;
    thread_cur_label(&th_l);
    thread_cur_clearance(&th_cl);
        
    gate_create(start_env->shared_container,"user gate", &th_l, 
                &th_cl, &user_gate, 0);

    gate_create(start_env->shared_container,"client gate", &th_l, 
                &th_cl, &client_gate, 0);
    
    gate_create(start_env->shared_container,"admin gate", &th_l, 
		&th_cl, &admin_gate, 0);
    
    printf("omd: init\n");

    thread_halt();
    return 0;    
}
