extern "C" {
#include <inc/lib.h>
#include <inc/authd.h>
#include <inc/error.h>
#include <inc/gateparam.h>
#include <inc/string.h>
#include <inc/sha1.h>
#include <inc/syscall.h>

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
static uint64_t root_taint;
static uint64_t root_grant;
static int64_t authd_grant;
static cobj_ref user_list_seg;
static cobj_ref base_as_ref;

struct user_entry {
    int valid;
    char name[16];
    uint64_t uid;
    uint64_t user_ct;
    cobj_ref user_gate;
};

struct user_list {
    pthread_mutex_t mu;
    user_entry users[1];
};

struct user_private {
    char pwhash[20];

    uint64_t grant;
    uint64_t taint;
};

static void __attribute__((noreturn))
authd_user_entry(void *arg, gate_call_data *parm, gatesrv_return *gr)
{
    uint64_t user_ct = (uint64_t) arg;
    authd_req *req = (authd_req *) &parm->param_buf[0];
    authd_reply *reply = (authd_reply *) &parm->param_buf[0];

    try {
	if (req->op != authd_chpass && req->op != authd_login)
	    throw error(-E_INVAL, "unknown op %d\n", req->op);

	int64_t seg_id;
	error_check(seg_id = container_find(user_ct, kobj_segment,
					    "user private"));

    	user_private *u = 0;
	uint32_t map_flags =
	    SEGMAP_READ | (req->op == authd_chpass ? SEGMAP_WRITE : 0);
    	error_check(segment_map(COBJ(user_ct, seg_id), map_flags,
				(void **) &u, 0));
    	scope_guard<int, void *> unmap(segment_unmap, u);

	sha1_ctx sctx;
	sha1_init(&sctx);
	sha1_update(&sctx, (unsigned char *) req->pass,
		    MIN(strlen(req->pass), sizeof(req->pass)));

	char pwhash[20];
	sha1_final((unsigned char *) &pwhash[0], &sctx);

    	if (memcmp(pwhash, u->pwhash, sizeof(u->pwhash)))
    	    throw error(-E_INVAL, "bad password");

    	if (req->op == authd_chpass) {
	    sha1_init(&sctx);
	    sha1_update(&sctx, (unsigned char *) req->npass,
			MIN(strlen(req->npass), sizeof(req->npass)));
	    sha1_final((unsigned char *) &u->pwhash[0], &sctx);
	}

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
alloc_user(const char *uname, const char *pass,
	   uint64_t g, uint64_t t, uint64_t uid,
	   user_entry *ue)
{
    label uct_l(1);
    uct_l.set(g, 0);

    int64_t user_ct;
    error_check(user_ct = sys_container_alloc(users_ct, uct_l.to_ulabel(),
					      "user ct", 0, CT_QUOTA_INF));
    scope_guard<int, cobj_ref> ct_drop(sys_obj_unref, COBJ(users_ct, user_ct));

    strncpy(&ue->name[0], uname, sizeof(ue->name));
    ue->user_ct = user_ct;
    ue->uid = uid;

    label useg_l(1);
    useg_l.set(authd_grant, 0);
    useg_l.set(g, 0);
    useg_l.set(t, 3);

    user_private *up = 0;
    cobj_ref up_seg;
    error_check(segment_alloc(user_ct, sizeof(*up), &up_seg,
			      (void **) &up, useg_l.to_ulabel(), "user private"));
    scope_guard<int, void *> up_unmap(segment_unmap, up);

    sha1_ctx sctx;
    sha1_init(&sctx);
    sha1_update(&sctx, (unsigned char *) pass, strlen(pass));
    sha1_final((unsigned char *) &up->pwhash[0], &sctx);
    up->grant = g;
    up->taint = t;

    label gt_c(2);
    label gt_l(1);
    gt_l.set(g, LB_LEVEL_STAR);
    gt_l.set(t, LB_LEVEL_STAR);
    gt_l.set(authd_grant, LB_LEVEL_STAR);
    gt_l.set(start_env->process_taint, LB_LEVEL_STAR);

    gatesrv_descriptor gd;
    gd.gate_container_ = user_ct;
    gd.name_ = "user auth gate";
    gd.as_ = base_as_ref;
    gd.label_ = &gt_l;
    gd.clearance_ = &gt_c;
    gd.func_ = &authd_user_entry;
    gd.arg_ = (void *) user_ct;

    ue->user_gate = gate_create(&gd);
    ue->valid = 1;
    ct_drop.dismiss();
}

static void
create_user(const char *uname, const char *pass,
	    uint64_t *ug, uint64_t *ut, uint64_t uid, user_entry *ue)
{
    int64_t g, t;
    error_check(g = sys_handle_create());
    error_check(t = sys_handle_create());

    alloc_user(uname, pass, g, t, uid, ue);

    *ug = g;
    *ut = t;
}

static void
authd_dispatch(authd_req *req, authd_reply *reply)
{
    if (req->op != authd_adduser && req->op != authd_getuser && req->op != authd_deluser)
	throw error(-E_BAD_OP, "unknown op %d", req->op);

    user_list *ul = 0;
    uint64_t ul_bytes = 0;
    error_check(segment_map(user_list_seg, SEGMAP_READ | SEGMAP_WRITE,
			    (void **) &ul, &ul_bytes));
    scope_guard<int, void *> ul_unmap(segment_unmap, ul);
    scoped_pthread_lock l(&ul->mu);

    user_entry *ue, *ue_match = 0;
    for (ue = &ul->users[0]; ue < (user_entry *) (((char *) ul) + ul_bytes); ue++) {
	if (!strcmp(ue->name, req->user) && ue->valid) {
	    ue_match = ue;
	    break;
	}
    }

    if (req->op == authd_adduser || req->op == authd_deluser) {
	label v;
	thread_cur_verify(&v);

	label root_v(3);
	root_v.set(root_grant, 0);
	error_check(v.compare(&root_v, label::leq_starlo));
    }

    if (req->op == authd_deluser) {
	if (!ue_match)
	    throw error(-E_NOT_FOUND, "no such user");
	sys_obj_unref(COBJ(users_ct, ue_match->user_ct));
	ue_match->valid = 0;
    }

    if (req->op == authd_adduser) {
	if (ue_match)
	    throw error(-E_EXISTS, "user already exists");

	uint64_t max_uid = 0;
	for (ue = &ul->users[0]; ue < (user_entry *) (((char *) ul) + ul_bytes); ue++) {
	    if (ue->valid && ue->uid > max_uid)
		max_uid = ue->uid;
	    if (!ue->valid)
		ue_match = ue;
	}

	int64_t cur_len = 0;
	if (!ue_match) {
	    error_check(cur_len = sys_segment_get_nbytes(user_list_seg));
	    error_check(sys_segment_resize(user_list_seg, cur_len + sizeof(*ue), 0));
	}

	user_list *ul2 = 0;
	error_check(segment_map(user_list_seg, SEGMAP_READ | SEGMAP_WRITE,
				(void **) &ul2, 0));
	scope_guard<int, void *> ul2_unmap(segment_unmap, ul2);

	if (!ue_match)
	    ue = (user_entry *) (((char *) ul2) + cur_len);

	create_user(req->user, req->pass,
		    &reply->user_grant, &reply->user_taint,
		    max_uid + 1, ue);
	reply->user_id = max_uid + 1;
	reply->user_gate = ue->user_gate;
    }

    if (req->op == authd_getuser) {
	if (!ue_match)
	    throw error(-E_NOT_FOUND, "no such user");

	reply->user_gate = ue_match->user_gate;
	reply->user_id = ue_match->uid;
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
authd_init(uint64_t rg)
{
    error_check(sys_self_get_as(&base_as_ref));

    int64_t rt;
    error_check(rt = sys_handle_create());
 
    root_grant = rg;
    root_taint = rt;

    error_check(authd_grant = sys_handle_create());

    label u_ctm(1);
    u_ctm.set(authd_grant, 0);
    u_ctm.set(start_env->process_taint, 3);
    int64_t ct;
    error_check(ct = sys_container_alloc(start_env->shared_container,
					 u_ctm.to_ulabel(), "users",
					 0, CT_QUOTA_INF));
    users_ct = ct;

    user_list *ul = 0;
    error_check(segment_alloc(users_ct, sizeof(*ul), &user_list_seg,
			      (void **) &ul, u_ctm.to_ulabel(), "user list"));
    scope_guard<int, void *> unmap(segment_unmap, ul);

    alloc_user("root", "", root_grant, root_taint, 0, &ul->users[0]);

    label th_ctm, th_clr;
    thread_cur_label(&th_ctm);
    thread_cur_clearance(&th_clr);
    th_ctm.set(root_grant, 1);
    th_ctm.set(root_taint, 1);
    th_ctm.set(start_env->process_grant, 1);

    gate_create(start_env->shared_container,
		"authd", &th_ctm, &th_clr,
		&authd_entry, 0);
}

int
main(int ac, char **av)
{
    try {
	if (ac != 2) {
	    cprintf("Usage: %s root-grant-handle\n", av[0]);
	    return -1;
	}

	uint64_t rg;
	error_check(strtou64(av[1], 0, 10, &rg));

    	authd_init(rg);
    	thread_halt();
    } catch (std::exception &e) {
    	printf("authd: %s\n", e.what());
    	return -1;
    }
}
