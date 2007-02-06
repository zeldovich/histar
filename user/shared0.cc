extern "C" {
#include <inc/gateparam.h>
#include <inc/syscall.h>
#include <inc/error.h>
#include <inc/stdio.h>
#include <inc/debug.h>
#include <inc/assert.h>

#include <inc/dis/share.h>

#include <inc/dis/catdir.h>

#include <string.h>
}

#include <inc/dis/globallabel.hh>
#include <inc/dis/labeltrans.hh>
#include <inc/dis/sharedutil.hh>

#include <inc/labelutil.hh>
#include <inc/gateclnt.hh>
#include <inc/gatesrv.hh>
#include <inc/error.hh>
#include <inc/scopeguard.hh>

static const char debug_user = 1;
static const char debug_client = 1;
static const char debug_admin = 1;
static const char debug_label = 1;

static struct catdir *catdir;

extern const char *__progname;

static label_trans *trans;

static const int num_servers = 10;
static const int num_clients = 10;
static struct {
    struct { 
	uint64_t id;
	cobj_ref gate;
    } servers[num_servers];
    struct { 
	uint64_t id;
	cobj_ref gate;
    } clients[num_clients];
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

static int64_t
get_grant_gate(uint64_t cat)
{
    char buf[16];
    sprintf(buf, "%ld", cat);
    return container_find(local_grants, kobj_gate, buf);
}


///////
// user
///////

static struct cobj_ref
create_grant_gate(uint64_t cat)
{
    char buffer[32];
    label th_l, th_cl(2);
    thread_cur_label(&th_l);
    th_cl.set(start_env->process_grant, 0);
    
    debug_print(debug_label, "cat %ld", cat);
    
    sprintf(buffer, "%ld", cat);
    return gate_create(local_grants, buffer, &th_l, &th_cl, 
		       &grant_cat, (void *)cat);
}

static void
user_add_local_cat(share_args *args)
{
    uint64_t cat = args->add_local_cat.cat;

    create_grant_gate(cat);

    struct global_cat gcat;
    gcat.k = principal_id;
    gcat.original = cat;
    error_check(catdir_insert(catdir, cat, &gcat));
}

static int64_t 
get_local_demand(global_cat *gcat, void *arg)
{
    uint64_t local;
    int r;
    struct catdir *dir = (struct catdir*) arg;
    if ((r = catdir_lookup_local(dir, gcat, &local)) == -E_NOT_FOUND) {
	// create a new foreign category
	local = handle_alloc();
	create_grant_gate(local);
	error_check(catdir_insert(dir, local, gcat));
	return local;
    } else if (r < 0)
	return r;
    
    return local;
}

static void 
get_global(uint64_t local, global_cat *gcat)
{
    error_check(catdir_lookup_global(catdir, local, gcat));
}

static void
user_observe(share_args *args, uint64_t taint_ct)
{
    cobj_ref gate = COBJ(0,0);
    for (int i = 0; i < num_servers; i++) {
	if (admin_info.servers[i].id == args->observe.id) {
	    gate = admin_info.servers[i].gate;
	    break;
	}
    }
    
    if (!gate.object)
	throw error(E_NOT_FOUND,"server %ld", args->observe.id);
    
    struct gate_call_data gcd;
    struct share_server_args *sargs = (struct share_server_args *)gcd.param_buf;
    sargs->op = share_server_read;
    sargs->offset = args->observe.offset;
    sargs->count = args->observe.count;    
    memcpy(&sargs->resource, &args->observe.res, sizeof(sargs->resource));
   
    gate_call gc(gate, 0, 0, 0);
    gc.call(&gcd, 0);

    if (sargs->ret < 0) {
	args->ret = sargs->ret;
	return;
    }

    global_label gl(sargs->label);
    debug_print(debug_label, "seg global %s", gl.string_rep());

    
    
    label l(1);
    l.copy_from(gl.local_label(get_local_demand, catdir));
    debug_print(debug_label, "seg local %s", l.to_string());
    int64_t seg_id;
    error_check(seg_id = sys_segment_copy(sargs->data_seg, taint_ct, 
					  l.to_ulabel(), "read seg"));
    args->ret = sargs->ret;
    args->ret_obj = COBJ(taint_ct, seg_id);
}

static void
user_modify(share_args *args)
{
    cobj_ref gate = COBJ(0,0);
    for (int i = 0; i < num_servers; i++) {
	if (admin_info.servers[i].id == args->observe.id) {
	    gate = admin_info.servers[i].gate;
	    break;
	}
    }
    
    if (!gate.object)
	throw error(E_NOT_FOUND,"server %ld", args->observe.id);
    
    struct ulabel *ul = label_alloc();
    scope_guard<void, struct ulabel*> seg_unmap(label_free, ul);
    sys_self_get_verify(ul);
    debug_print(debug_label, "verify local %s", label_to_string(ul));
    global_label gl(ul, &get_global);
    debug_print(debug_label, "verify global %s", gl.string_rep());

    struct gate_call_data gcd;
    struct share_server_args *sargs = (struct share_server_args *)gcd.param_buf;
    sargs->op = share_server_write;
    sargs->offset = args->modify.offset;
    sargs->count = args->modify.count;
    sargs->data_seg = args->modify.seg;
    memcpy(&sargs->resource, &args->modify.res, sizeof(sargs->resource));
    assert(sizeof(sargs->label) > (uint32_t)gl.serial_len());
    memcpy(sargs->label, gl.serial(), gl.serial_len());

    label dl(3);
    dl.set(args->modify.taint, LB_LEVEL_STAR);
    gate_call gc(gate, 0, &dl, 0);
    gc.call(&gcd, 0);
    
    args->ret = sargs->ret;    
}

level_t
taint_to(level_t l, int arg)
{
    if (l != 1)
	return arg;
    return l;
}

static void __attribute__((noreturn)) 
    user_gate(void *arg, struct gate_call_data *parm, gatesrv_return *gr);



static void
user_create_gate(share_args * args)
{
    global_label gl(args->user_gate.label);
    debug_print(debug_label, "global %s", gl.string_rep());
    label l(1);
    l.copy_from(gl.local_label(get_local_demand, catdir));
    debug_print(debug_label, "local %s", l.to_string());

    label th_l(1);
    thread_cur_label(&th_l);
    
    struct ulabel *ul = l.to_ulabel();
    for (uint32_t i = 0; i < ul->ul_nent; i++) {
	uint64_t h = LB_HANDLE(ul->ul_ent[i]);
	if (LB_LEVEL(ul->ul_ent[i]) != 1 && th_l.get(h) != LB_LEVEL_STAR) {
	    uint64_t gt = get_grant_gate(h);
	    gate_send(COBJ(local_grants, gt), 0, 0, 0);
	    th_l.set(h, LB_LEVEL_STAR);
	}
    }
    debug_print(debug_label, "thread local %s", th_l.to_string());
    
    label th_cl(2);
    thread_cur_clearance(&th_cl);
    args->user_gate.gt = gate_create(args->user_gate.ct, "user gate", &th_l, 
				     &th_cl, &user_gate, 0);
    return;
}

static void
user_get_label(share_args *args)
{
    cobj_ref gate = COBJ(0,0);
    for (int i = 0; i < num_servers; i++) {
	if (admin_info.servers[i].id == args->get_label.id) {
	    gate = admin_info.servers[i].gate;
	    break;
	}
    }
    
    if (!gate.object)
	throw error(E_NOT_FOUND,"server %ld", args->observe.id);
    
    struct gate_call_data gcd;
    struct share_server_args *sargs = (struct share_server_args *)gcd.param_buf;
    sargs->op = share_server_label;
    memcpy(&sargs->resource, &args->get_label.res, sizeof(sargs->resource));
   
    gate_call gc(gate, 0, 0, 0);
    gc.call(&gcd, 0);

    if (sargs->ret < 0) {
	args->ret = sargs->ret;
	return;
    }
    
    memcpy(args->get_label.label, sargs->label, sizeof(args->get_label.label));
    args->ret = sargs->ret;    
}

static void
user_gate_enter(share_args *args, label *cs, label *ds, label *dr)
{
    cobj_ref gate = COBJ(0,0);
    for (int i = 0; i < num_servers; i++) {
	if (admin_info.servers[i].id == args->gate_call.id) {
	    gate = admin_info.servers[i].gate;
	    break;
	}
    }
    
    if (!gate.object)
	throw error(E_NOT_FOUND,"server %ld", args->observe.id);
    
    struct ulabel *ul = label_alloc();
    scope_guard<void, struct ulabel*> free_label(label_free, ul);
    sys_self_get_verify(ul);
    debug_print(debug_label, "verify local %s", label_to_string(ul));
    global_label gl(ul, &get_global);
    debug_print(debug_label, "verify global %s", gl.string_rep());

    struct gate_call_data gcd;
    struct share_server_args *sargs = (struct share_server_args *)gcd.param_buf;
    sargs->op = share_server_gate_call;
    sargs->data_seg = args->gate_call.seg;

    assert(sizeof(sargs->resource) > strlen(args->gate_call.pn) + 1);
    memcpy(&sargs->resource, &args->gate_call.pn, sizeof(sargs->resource));
    assert(sizeof(sargs->label) > (uint32_t)gl.serial_len());
    memcpy(sargs->label, gl.serial(), gl.serial_len());

    label dl(3);
    dl.set(args->gate_call.taint, LB_LEVEL_STAR);
    gate_call gc(gate, 0, &dl, 0);
    gc.call(&gcd, 0);

    struct rgc_return *r = 0;
    error_check(segment_map(sargs->data_seg, 0, SEGMAP_READ, 
			    (void **)&r, 0, 0));
    scope_guard<int, void*> seg_unmap(segment_unmap, r);
    
    global_label gcs(r->cs);
    global_label gds(r->ds);
    global_label gdr(r->dr);

    debug_print(debug_label, "global cs %s", gcs.string_rep());
    debug_print(debug_label, "global ds %s", gds.string_rep());
    debug_print(debug_label, "global dr %s", gdr.string_rep());
    
    cs->copy_from(gcs.local_label(get_local_demand, catdir));
    ds->copy_from(gds.local_label(get_local_demand, catdir));
    dr->copy_from(gdr.local_label(get_local_demand, catdir));

    debug_print(debug_label, "local cs %s", cs->to_string());
    debug_print(debug_label, "local ds %s", ds->to_string());
    debug_print(debug_label, "local dr %s", dr->to_string());

    args->ret = sargs->ret;    
    return;
}

static void __attribute__((noreturn))
user_gate(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    gate_call_data bck;
    gate_call_data_copy(&bck, parm);
    share_args *args = (share_args*) parm->param_buf;

    debug_print(debug_user, "%s: op %d", __progname, args->op);

    label *cs = new label(0);
    label *ds = new label(3);
    label *dr = new label(0);

    try {
	switch(args->op) {
	case share_observe:
	    user_observe(args, parm->taint_container);
	    break;
	case share_modify:
	    user_modify(args);
	    break;
	case share_get_label:
	    user_get_label(args);
	    break;
	case share_user_gate:
	    user_create_gate(args);
	    break;
	case share_add_local_cat:
	    user_add_local_cat(args);
	    break;
	case share_gate_call:
	    user_gate_enter(args, cs, ds, dr);
	    break;
	default:
	    throw basic_exception("unknown op %d", args->op);
	}
    } catch (basic_exception e) {
	cprintf("user_gate: %s\n", e.what());
	args->ret = -1;
    }
    gate_call_data_copy(parm, &bck);
    gr->ret(cs, ds, dr);    
}

///////
// client
///////

static void
grant_cats(struct ulabel *ul, label *dl)
{
    for (uint32_t i = 0; i < ul->ul_nent; i++) {
	uint64_t cat = LB_HANDLE(ul->ul_ent[i]);
	uint64_t gt = get_grant_gate(cat);
	gate_send(COBJ(local_grants, gt), 0, 0, 0);
	dl->set(cat, LB_LEVEL_STAR);
    }
}

static void
client_grant_label(share_args *args, label *dl)
{
    global_label gl(args->grant.label);
    label l;
    trans->local_for(&gl, &l);
    grant_cats(l.to_ulabel(), dl);
    debug_print(debug_label, "grant label %s", l.to_string());
    args->ret = 0;
}

static void
client_localize(share_args *args)
{
    global_label gl(args->localize.label);
    
    debug_print(debug_label, "global %s", gl.string_rep());
    label l(1);
    l.copy_from(gl.local_label(get_local_demand, catdir));
    debug_print(debug_label, "local %s", l.to_string());

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
	case share_grant_label:
	    client_grant_label(args, dl);
	    break;
	case share_localize:
	    client_localize(args);
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

    for (int i = 0; i < num_servers; i++) {
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
    for (int i = 0; i < num_clients; i++) {
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
    struct gate_call_data gcd;
    static_assert(sizeof(struct share_args) <= sizeof(gcd.param_buf));

    memset(&admin_info, 0, sizeof(admin_info));
    
    label l(1);
    l.set(start_env->process_grant, 0);
    int64_t ct;
    error_check(ct = sys_container_alloc(start_env->shared_container, 
					 l.to_ulabel(), "cats", 
					 0, CT_QUOTA_INF));
    local_grants = ct;
    
    struct cobj_ref cobj;
    error_check(catdir_alloc(ct, 16, &cobj, &catdir, l.to_ulabel(), "catdir"));
    trans = new label_trans(cobj);

    label th_l, th_cl;
    thread_cur_label(&th_l);
    thread_cur_clearance(&th_cl);
        
    gate_create(start_env->shared_container,"user gate", &th_l, 
                &th_cl, &user_gate, 0);

    gate_create(start_env->shared_container,"client gate", &th_l, 
                &th_cl, &client_gate, 0);
    
    gate_create(start_env->shared_container,"admin gate", &th_l, 
		&th_cl, &admin_gate, 0);
    
    printf("%s: init\n", av[0]);


    thread_halt();
    return 0;
}
