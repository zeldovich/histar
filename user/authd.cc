extern "C" {
#include <inc/lib.h>
#include <inc/authd.h>
#include <inc/error.h>
#include <inc/queue.h>
#include <inc/gateparam.h>

#include <string.h>
}

#include <inc/gatesrv.hh>
#include <inc/gateclnt.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/pthread.hh>
#include <inc/scopeguard.hh>
#include <inc/error.hh>

static uint64_t users_ct;

struct user_gate {
    LIST_ENTRY(user_gate) ug_link;
    char ug_name[16];
    struct cobj_ref ug_seg;
    gatesrv *ug_gate;
};
LIST_HEAD(user_gates, user_gate) ug_head;

struct user {
    char name[16];
    char pass[16];
    uint64_t grant;
    uint64_t taint;
};

static struct user_gate *
user_gate_find(char *uname)
{
    struct user_gate *ug;
    LIST_FOREACH(ug, &ug_head, ug_link)
	if (!strcmp(ug->ug_name, uname))
	    return ug;
    return 0;
}

static void __attribute__((noreturn))
authd_user_entry(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    user_gate *ugate = (user_gate *) arg;
    authd_req *req = (authd_req *) &parm->param_buf[0];
    authd_reply *reply = (authd_reply *) &parm->param_buf[0];

    try {
    	struct user *u = 0;
    	error_check(segment_map(ugate->ug_seg, SEGMAP_READ | SEGMAP_WRITE, (void **)&u, 0));
    	scope_guard<int, void *> unmap(segment_unmap, u);
    
    	if (strcmp(req->pass, u->pass))
    	    throw error(-E_INVAL, "bad password");
    	if (req->op == authd_chpass)
    	    memcpy(&u->pass[0], &req->npass[0], sizeof(u->pass));
    
    	label *ds = new label(3);
    	ds->set(u->grant, LB_LEVEL_STAR);
    	ds->set(u->taint, LB_LEVEL_STAR);
    
    	reply->err = 0;
    	reply->user_taint = u->taint;
    	reply->user_grant = u->grant;
    
    	gr->ret(0, ds, 0);
    } catch (error &e) {
    	cprintf("authd_user_entry: %s\n", e.what());
    	reply->err = e.err();
    } catch (std::exception &e) {
    	cprintf("authd_user_entry: %s\n", e.what());
    	reply->err = -E_INVAL;
    }
    gr->ret(0, 0, 0);
}

static void
create_user(char *uname, char *pass, uint64_t *ug, uint64_t *ut)
{
    user_gate *ugate = new user_gate();
    scope_guard<void, user_gate *> ugate_drop(delete_obj, ugate);
    memcpy(&ugate->ug_name[0], uname, sizeof(ugate->ug_name));

    int64_t g = sys_handle_create();
    int64_t t = sys_handle_create();
    error_check(g);
    error_check(t);

    *ug = g;
    *ut = t;

    label l(1);
    l.set(g, 0);
    l.set(t, 3);
    l.set(start_env->process_grant, 0);
    l.set(start_env->process_taint, 3);

    struct user *u = 0;
    error_check(segment_alloc(users_ct, sizeof(*u),
			      &ugate->ug_seg, (void **) &u,
			      l.to_ulabel(), uname));
    scope_guard<int, struct cobj_ref> useg_drop(sys_obj_unref, ugate->ug_seg);
    scope_guard<int, void *> unmap(segment_unmap, u);

    u->grant = g;
    u->taint = t;
    memcpy(&u->name[0], uname, sizeof(u->name));
    memcpy(&u->pass[0], pass, sizeof(u->pass));

    label th_ctm, th_clr;
    thread_cur_label(&th_ctm);
    thread_cur_clearance(&th_clr);
    th_clr.set(start_env->process_grant, 0);

    ugate->ug_gate = new gatesrv(users_ct, uname, &th_ctm, &th_clr);
    ugate->ug_gate->set_entry_container(start_env->proc_container);
    ugate->ug_gate->set_entry_function(&authd_user_entry, (void *) ugate);
    ugate->ug_gate->enable();

    ugate_drop.dismiss();
    useg_drop.dismiss();

    LIST_INSERT_HEAD(&ug_head, ugate, ug_link);
}

static void
authd_dispatch(authd_req *req, authd_reply *reply)
{
    static pthread_mutex_t users_mu;

    if (req->op == authd_adduser) {
    	scoped_pthread_lock l(&users_mu);
    
    	struct user_gate *ug = user_gate_find(req->user);
    	if (ug)
    	    throw error(-E_EXISTS, "user already exists");
    
    	create_user(req->user, req->pass, &reply->user_grant, &reply->user_taint);
    } else if (req->op == authd_deluser) {
    	scoped_pthread_lock l(&users_mu);
    
    	struct user_gate *ug = user_gate_find(req->user);
    	if (!ug)
    	    throw error(-E_INVAL, "user does not exist");
    
    	delete ug->ug_gate;
    	sys_obj_unref(ug->ug_seg);
    	LIST_REMOVE(ug, ug_link);
    	delete ug;
    } else if (req->op == authd_login || req->op == authd_chpass) {
    	struct user_gate *ug = user_gate_find(req->user);
    	if (!ug)
    	    throw error(-E_INVAL, "user does not exist");
    
    	gate_call_data gcd;
    	authd_req *lreq = (authd_req *) &gcd.param_buf[0];
    	authd_reply *lrep = (authd_reply *) &gcd.param_buf[0];
    
    	memcpy(lreq, req, sizeof(*lreq));
    	gate_call(ug->ug_gate->gate(), &gcd, 0, 0, 0);

    	if (lrep->err)
    	    throw error(lrep->err, "response from authd_login");
    
    	reply->user_taint = lrep->user_taint;
    	reply->user_grant = lrep->user_grant;
    } else {
	   throw error(-E_BAD_OP, "unknown op %d", req->op);
    }
}

static void __attribute__((noreturn))
authd_entry(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    label *ds = 0;
    authd_req req = *(authd_req *) &parm->param_buf[0];
    authd_reply reply;
    memset(&reply, 0, sizeof(reply));

    req.user[sizeof(req.user) - 1] = '\0';
    req.pass[sizeof(req.pass) - 1] = '\0';

    try {
    	authd_dispatch(&req, &reply);
    	if (reply.user_taint && reply.user_grant) {
        	    ds = new label(3);
        	    ds->set(reply.user_taint, LB_LEVEL_STAR);
        	    ds->set(reply.user_grant, LB_LEVEL_STAR);
	   }
    } catch (error &e) {
    	cprintf("authd_entry: %s\n", e.what());
    	reply.err = e.err();
    } catch (std::exception &e) {
    	cprintf("authd_entry: %s\n", e.what());
    	reply.err = -E_INVAL;
    }

    memcpy(&parm->param_buf[0], &reply, sizeof(reply));
    if (reply.err) {
    	if (ds)
    	    delete ds;
    	ds = 0;
    }

    gr->ret(0, ds, 0);
}

static void
authd_init()
{
    label u_ctm(1);
    u_ctm.set(start_env->process_grant, 0);
    u_ctm.set(start_env->process_taint, 3);
    int64_t ct = sys_container_alloc(start_env->shared_container,
				     u_ctm.to_ulabel(), "users");
    error_check(ct);
    users_ct = ct;

    label th_ctm, th_clr;
    thread_cur_label(&th_ctm);
    thread_cur_clearance(&th_clr);

    gatesrv *g = new gatesrv(start_env->shared_container,
			     "authd",
			     &th_ctm, &th_clr);
    g->set_entry_container(start_env->proc_container);
    g->set_entry_function(&authd_entry, 0);
    g->enable();
    
    uint64_t ug;
    uint64_t ut;
    create_user((char*)"root", (char*)"", &ug, &ut);
}

int
main(int ac, char **av)
{
    try {
	authd_init();
	thread_halt();
    } catch (std::exception &e) {
	printf("authd: %s\n", e.what());
	return -1;
    }
}
