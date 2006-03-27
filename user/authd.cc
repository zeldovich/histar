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
static uint64_t groups_ct;
static uint64_t root_taint;
static uint64_t root_grant;

static uint64_t id_count;

struct gate_data {
    LIST_ENTRY(gate_data) gd_link;
    char gd_name[16];
    struct cobj_ref gd_seg;
    gatesrv *gd_gate;
};
LIST_HEAD(user_gates, gate_data) ug_head;
LIST_HEAD(group_gates, gate_data) gg_head;

struct user {
    uint64_t id;
    char name[16];
    char pass[16];
    
    uint64_t grant;
    uint64_t taint;
};

struct group {
    uint64_t id;
    char name[16];  
    
    uint64_t user_grant;
    uint64_t taint;
    uint64_t member_count;
    uint64_t member_size;
    struct {
        int type;
        uint64_t t;
        uint64_t g;
    } member[16];
};

static struct gate_data *
gate_data_find(char *uname)
{
    struct gate_data *ug;
    LIST_FOREACH(ug, &ug_head, gd_link)
	if (!strcmp(ug->gd_name, uname))
	    return ug;
    return 0;
}

static struct gate_data *
group_gate_find(char *gname)
{
    struct gate_data *gg;
    LIST_FOREACH(gg, &gg_head, gd_link)
    if (!strcmp(gg->gd_name, gname))
        return gg;
    return 0;
}

static char
at_star(uint64_t taint, uint64_t grant)
{
    label l;
    thread_cur_label(&l);
    return (l.get(taint) == LB_LEVEL_STAR &&
        l.get(grant) == LB_LEVEL_STAR);
}

static void __attribute__((noreturn))
authd_user_entry(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    gate_data *ugate = (gate_data *) arg;
    authd_req *req = (authd_req *) &parm->param_buf[0];
    authd_reply *reply = (authd_reply *) &parm->param_buf[0];

    try {
    	struct user *u = 0;
    	error_check(segment_map(ugate->gd_seg, SEGMAP_READ | SEGMAP_WRITE, (void **)&u, 0));
    	scope_guard<int, void *> unmap(segment_unmap, u);
	
        if (req->op == authd_getuid) {
            reply->user_id = u->id;
            reply->user_taint = u->taint;
            reply->user_grant = u->grant;
            reply->err = 0;
            gr->ret(0, 0, 0);
        }
	
    	if (strcmp(req->pass, u->pass))
    	    throw error(-E_INVAL, "bad password");
    	if (req->op == authd_chpass)
    	    memcpy(&u->pass[0], &req->npass[0], sizeof(u->pass));
    
    	label *ds = new label(3);
    	ds->set(u->grant, LB_LEVEL_STAR);
    	ds->set(u->taint, LB_LEVEL_STAR);
    
        label th_ctm, th_clr;
        thread_cur_label(&th_ctm);
        thread_cur_clearance(&th_clr);
    
    	reply->err = 0;
    	reply->user_taint = u->taint;
    	reply->user_grant = u->grant;
	    reply->user_id = u->id;

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

static void __attribute__((noreturn))
authd_group_entry(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    gate_data *ggate = (gate_data *) arg;
    authd_req *req = (authd_req *) &parm->param_buf[0];
    authd_reply *reply = (authd_reply *) &parm->param_buf[0];

    try {
        struct group *g = 0;
        if (req->op == authd_addtogroup) {
            error_check(segment_map(ggate->gd_seg, SEGMAP_READ | SEGMAP_WRITE, (void **)&g, 0));
            scope_guard<int, void *> unmap(segment_unmap, g);
            // XXX add to group mutex
            reply->err = 0;
            if (g->member_count == g->member_size) {
                reply->err = -E_NO_SPACE;
                gr->ret(0, 0, 0);
            }
            for (uint64_t i = 0; i < g->member_count; i++) {
                if (g->member[i].t == req->taint 
                 && g->member[i].g == req->grant)
                    throw error(-E_INVAL, "user already in group");   
            }
            g->member[g->member_count].t = req->taint;
            g->member[g->member_count].g = req->grant;
            g->member[g->member_count].type = req->type;
            g->member_count++;
            reply->err = 0;
            reply->user_grant = 0;
            reply->user_taint = 0;
            gr->ret(0, 0, 0);
        }
        else if (req->op == authd_logingroup) {
            error_check(segment_map(ggate->gd_seg, SEGMAP_READ, (void **)&g, 0));
            scope_guard<int, void *> unmap(segment_unmap, g);
            
            if (!at_star(req->taint, req->grant)) {
                reply->err = -E_INVAL;
                gr->ret(0, 0, 0);    
            }
            
            for (int i = 0; (uint64_t)i < g->member_count; i++) {
                if (g->member[i].t == req->taint 
                 && g->member[i].g == req->grant) {
                    label *ds = new label(3);
                    ds->set(g->taint, LB_LEVEL_STAR);
                    reply->user_taint = g->taint;
                    if (g->member[i].type == group_write) {
                        ds->set(g->user_grant, LB_LEVEL_STAR);
                        reply->user_grant = g->user_grant;
                    }
                    reply->err = 0;
                    gr->ret(0, ds, 0);
                }
            }
            throw error(-E_INVAL, "not a member of the group");
        }
        else if (req->op == authd_delfromgroup) {
            error_check(segment_map(ggate->gd_seg, SEGMAP_READ | SEGMAP_WRITE, (void **)&g, 0));
            scope_guard<int, void *> unmap(segment_unmap, g);     
            
            for (int i = 0; (uint64_t)i < g->member_count; i++) {
                if (g->member[i].t == req->taint 
                 && g->member[i].g == req->grant) {
                    memmove(&g->member[i], &g->member[i + 1], 
                            g->member_count - (i + 1));
                    g->member_count--;
                    reply->err = 0;
                    gr->ret(0, 0, 0);
                }
            }
            throw error (-E_INVAL, "not a member of the group");  
        }
        throw error(-E_BAD_OP, "unknown operation");
    } catch (error &e) {
        cprintf("authd_group_entry: %s\n", e.what());
        reply->err = e.err();
    } catch (std::exception &e) {
        cprintf("authd_group_entry: %s\n", e.what());
        reply->err = -E_INVAL;
    }
    gr->ret(0, 0, 0);
}

static void
alloc_user(char *uname, char *pass, uint64_t g, uint64_t t)
{
    gate_data *ugate = new gate_data();
    scope_guard<void, gate_data *> ugate_drop(delete_obj, ugate);
    memcpy(&ugate->gd_name[0], uname, sizeof(ugate->gd_name));

    label l(1);
    l.set(g, 0);
    l.set(t, 3);
    l.set(start_env->process_grant, 0);
    l.set(start_env->process_taint, 3);

    struct user *u = 0;
    error_check(segment_alloc(users_ct, sizeof(*u),
                  &ugate->gd_seg, (void **) &u,
                  l.to_ulabel(), uname));
    scope_guard<int, struct cobj_ref> useg_drop(sys_obj_unref, ugate->gd_seg);
    scope_guard<int, void *> unmap(segment_unmap, u);

    u->grant = g;
    u->taint = t;
    u->id = id_count++;
    memcpy(&u->name[0], uname, sizeof(u->name));
    memcpy(&u->pass[0], pass, sizeof(u->pass));

    label th_ctm, th_clr;
    thread_cur_label(&th_ctm);
    thread_cur_clearance(&th_clr);
    th_clr.set(start_env->process_grant, 0);

    ugate->gd_gate = new gatesrv(users_ct, uname, &th_ctm, &th_clr);
    ugate->gd_gate->set_entry_container(start_env->proc_container);
    ugate->gd_gate->set_entry_function(&authd_user_entry, (void *) ugate);
    ugate->gd_gate->enable();

    ugate_drop.dismiss();
    useg_drop.dismiss();

    LIST_INSERT_HEAD(&ug_head, ugate, gd_link);
}

static void
create_user(char *uname, char *pass, uint64_t *ug, uint64_t *ut)
{
    int64_t g = sys_handle_create();
    int64_t t = sys_handle_create();
    error_check(g);
    error_check(t);

    alloc_user(uname, pass, g, t);

    *ug = g;
    *ut = t;
}

static void
alloc_group(char *gname, uint64_t creator_grant, 
            uint64_t creator_taint, uint64_t taint)
{
    gate_data *ggate = new gate_data();
    scope_guard<void, gate_data *> ggate_drop(delete_obj, ggate);
    memcpy(&ggate->gd_name[0], gname, sizeof(ggate->gd_name));

    // authd can read group segment, creator+authd can write
    label l(1);
    l.set(creator_taint, 0);
    l.set(start_env->process_grant, 0);
    l.set(start_env->process_taint, 3);

    struct group *g = 0;
    error_check(segment_alloc(groups_ct, sizeof(*g),
                  &ggate->gd_seg, (void **) &g,
                  l.to_ulabel(), gname));
    scope_guard<int, struct cobj_ref> gseg_drop(sys_obj_unref, ggate->gd_seg);
    scope_guard<int, void *> unmap(segment_unmap, g);

    g->user_grant = creator_grant;
    g->taint = taint;
    g->id = id_count++;
    g->member_size = 16;
    g->member[0].type = group_write;
    g->member[0].t = creator_taint;
    g->member[0].g = creator_grant;
    g->member_count = 1;
    memcpy(&g->name[0], gname, sizeof(g->name));
    

    label th_ctm, th_clr;
    thread_cur_label(&th_ctm);
    thread_cur_clearance(&th_clr);
    th_clr.set(start_env->process_grant, 0);
    th_ctm.set(creator_taint, 1);
    
    ggate->gd_gate = new gatesrv(groups_ct, gname, &th_ctm, &th_clr);
    ggate->gd_gate->set_entry_container(start_env->proc_container);
    ggate->gd_gate->set_entry_function(&authd_group_entry, (void *) ggate);
    ggate->gd_gate->enable();

    ggate_drop.dismiss();
    gseg_drop.dismiss();

    LIST_INSERT_HEAD(&gg_head, ggate, gd_link);   
}

static void
create_group(char *gname, uint64_t creator_grant, 
             uint64_t creator_taint, uint64_t *taint)
{
    int64_t t = sys_handle_create();
    error_check(t);

    alloc_group(gname, creator_grant, creator_taint, t);
    *taint = t;
}

static void
authd_dispatch(authd_req *req, authd_reply *reply)
{
    static pthread_mutex_t users_mu;
    static pthread_mutex_t groups_mu;

    if (req->op == authd_adduser) {
        scoped_pthread_lock l(&users_mu);
    	struct gate_data *ug = gate_data_find(req->user);
    	if (ug)
    	    throw error(-E_EXISTS, "user already exists");
    
    	create_user(req->user, req->pass, &reply->user_grant, &reply->user_taint);
    } else if (req->op == authd_deluser) {
        scoped_pthread_lock l(&users_mu);
    
    	struct gate_data *ug = gate_data_find(req->user);
    	if (!ug)
    	    throw error(-E_INVAL, "user does not exist");
    
    	delete ug->gd_gate;
    	sys_obj_unref(ug->gd_seg);
    	LIST_REMOVE(ug, gd_link);
    	delete ug;
    } else if (req->op == authd_login || req->op == authd_chpass) {
    	struct gate_data *ug = gate_data_find(req->user);
    	if (!ug)
    	    throw error(-E_INVAL, "user does not exist");
    
    	gate_call_data gcd;
    	authd_req *lreq = (authd_req *) &gcd.param_buf[0];
    	authd_reply *lrep = (authd_reply *) &gcd.param_buf[0];
    
    	memcpy(lreq, req, sizeof(*lreq));
    	gate_call(ug->gd_gate->gate(), &gcd, 0, 0, 0, 0);

    	if (lrep->err)
    	    throw error(lrep->err, "response from authd_login");
    
    	reply->user_taint = lrep->user_taint;
    	reply->user_grant = lrep->user_grant;
        reply->user_id = lrep->user_id;
    } else if (req->op == authd_addgroup) {
        scoped_pthread_lock l(&groups_mu);
        struct gate_data *gg = group_gate_find(req->group);
        if (gg)
            throw error(-E_EXISTS, "group already exists");
    
        create_group(req->group, req->grant, req->taint, &reply->user_taint);
    } else if (req->op == authd_addtogroup || req->op == authd_delfromgroup 
            || req->op == authd_logingroup) {
        struct gate_data *gg = group_gate_find(req->group);
        if (!gg)
            throw error(-E_INVAL, "group does not exist");
    
        gate_call_data gcd;
        authd_req *lreq = (authd_req *) &gcd.param_buf[0];
        authd_reply *lrep = (authd_reply *) &gcd.param_buf[0];
    
        memcpy(lreq, req, sizeof(*lreq));
        
        label ds;
        thread_cur_label(&ds);
        gate_call(gg->gd_gate->gate(), &gcd, 0, &ds, 0, 0);

        if (lrep->err)
            throw error(lrep->err, "response from authd_login");
    
        reply->user_taint = lrep->user_taint;
        reply->user_grant = lrep->user_grant;
        reply->user_id = lrep->user_id;
    } else if (req->op == authd_getuid) {
        struct gate_data *ug;
        LIST_FOREACH(ug, &ug_head, gd_link) {
            gate_call_data gcd;
            authd_req *lreq = (authd_req *) &gcd.param_buf[0];
            authd_reply *lrep = (authd_reply *) &gcd.param_buf[0];
            
            memcpy(lreq, req, sizeof(*lreq));
            gate_call(ug->gd_gate->gate(), &gcd, 0, 0, 0, 0);
            
            if (lrep->err)
                throw error(lrep->err, "response from authd_login");
            if (at_star(lrep->user_taint, lrep->user_grant)) {
                reply->user_taint = lrep->user_taint;
                reply->user_grant = lrep->user_grant;
                reply->user_id = lrep->user_id;
                return;
            }
	   }
       // no current user
       reply->err = -E_INVAL; 
    } else if (req->op == authd_unamehandles) {
        struct gate_data *ug = gate_data_find(req->user);
        if (!ug)
            throw error(-E_INVAL, "user does not exist");
        gate_call_data gcd;
        authd_req *lreq = (authd_req *) &gcd.param_buf[0];
        authd_reply *lrep = (authd_reply *) &gcd.param_buf[0];
            
        memcpy(lreq, req, sizeof(*lreq));
        gate_call(ug->gd_gate->gate(), &gcd, 0, 0, 0, 0);
        
        if (lrep->err)
            throw error(lrep->err, "response from authd_login");
    
        reply->user_taint = lrep->user_taint;
        reply->user_grant = lrep->user_grant;
        reply->user_id = lrep->user_id;
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

    label th_ctm, th_clr;
    thread_cur_label(&th_ctm);
    thread_cur_clearance(&th_clr);

    try {
    	authd_dispatch(&req, &reply);
    	if (reply.user_taint || reply.user_grant) {
    	    ds = new label(3);
    	    if (reply.user_taint)
                ds->set(reply.user_taint, LB_LEVEL_STAR);
    	    if (reply.user_grant)
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
    int64_t rg = sys_handle_create();
    int64_t rt = sys_handle_create();
    error_check(rg);
    error_check(rt);
    
    root_grant = rg;
    root_taint = rt;
    
    // only authd can read users, only root+authd can write
    label u_ctm(1);
    u_ctm.set(start_env->process_grant, 0);
    u_ctm.set(start_env->process_taint, 3);
    u_ctm.set(root_taint, 0);
    int64_t ct = sys_container_alloc(start_env->shared_container,
                     u_ctm.to_ulabel(), "users");
    error_check(ct);
    users_ct = ct;
    alloc_user((char*)"root", (char*)"", root_grant, root_taint);
    
    // only authd can write groups
    label g_ctm(1);
    g_ctm.set(start_env->process_grant, 0);
    g_ctm.set(start_env->process_taint, 3);
    int64_t ct2 = sys_container_alloc(start_env->shared_container,
                     g_ctm.to_ulabel(), "groups");
    error_check(ct2);
    groups_ct = ct2;

    label th_ctm, th_clr;
    thread_cur_label(&th_ctm);
    thread_cur_clearance(&th_clr);
    th_ctm.set(root_grant, 1);
    th_ctm.set(root_taint, 1);
    
    gatesrv *g = new gatesrv(start_env->shared_container,
                 "authd",
                 &th_ctm, &th_clr);
    g->set_entry_container(start_env->proc_container);
    g->set_entry_function(&authd_entry, 0);
    g->enable();   
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
