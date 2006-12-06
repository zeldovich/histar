extern "C" {
#include <inc/lib.h>
#include <inc/authd.h>
#include <inc/error.h>
#include <inc/gateparam.h>
#include <inc/string.h>
#include <inc/syscall.h>
#include <inc/stdio.h>

#include <string.h>
}

#include <inc/gatesrv.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/jthread.hh>
#include <inc/scopeguard.hh>
#include <inc/error.hh>
#include <inc/authclnt.hh>

static uint64_t root_grant;
static int64_t auth_dir_grant;
static cobj_ref user_list_seg;

struct user_entry {
    int valid;
    char name[16];
    cobj_ref user_gate;
};

struct user_list {
    jthread_mutex_t mu;
    user_entry users[1];
};

static void
auth_dir_dispatch(auth_dir_req *req, auth_dir_reply *reply)
{
    char log_msg[256];
    if (req->op != auth_dir_lookup && req->op != auth_dir_add && req->op != auth_dir_remove)
	throw error(-E_BAD_OP, "unknown op %d", req->op);

    user_list *ul = 0;
    uint64_t ul_bytes = 0;
    error_check(segment_map(user_list_seg, 0, SEGMAP_READ | SEGMAP_WRITE,
			    (void **) &ul, &ul_bytes, 0));
    scope_guard<int, void *> ul_unmap(segment_unmap, ul);
    scoped_jthread_lock l(&ul->mu);

    user_entry *ue, *ue_match = 0;
    for (ue = &ul->users[0]; ue < (user_entry *) (((char *) ul) + ul_bytes); ue++) {
	if (!strcmp(ue->name, req->user) && ue->valid) {
	    ue_match = ue;
	    break;
	}
    }

    if (req->op == auth_dir_add || req->op == auth_dir_remove) {
	label v;
	thread_cur_verify(&v);

	label root_v(3);
	root_v.set(root_grant, 0);
	error_check(v.compare(&root_v, label::leq_starlo));
    }

    if (req->op == auth_dir_remove) {
	if (!ue_match)
	    throw error(-E_NOT_FOUND, "no such user");

	snprintf(&log_msg[0], sizeof(log_msg), "deleting user %s", req->user);
	auth_log(log_msg);

	ue_match->valid = 0;
    }

    if (req->op == auth_dir_add) {
	if (ue_match)
	    throw error(-E_EXISTS, "user already exists");

	for (ue = &ul->users[0]; ue < (user_entry *) (((char *) ul) + ul_bytes); ue++)
	    if (!ue->valid)
		ue_match = ue;

	int64_t cur_len = 0;
	if (!ue_match) {
	    error_check(cur_len = sys_segment_get_nbytes(user_list_seg));
	    error_check(sys_segment_resize(user_list_seg, cur_len + sizeof(*ue)));
	}

	user_list *ul2 = 0;
	error_check(segment_map(user_list_seg, 0, SEGMAP_READ | SEGMAP_WRITE,
				(void **) &ul2, 0, 0));
	scope_guard<int, void *> ul2_unmap(segment_unmap, ul2);

	if (ue_match)
	    ue = ue_match;
	else
	    ue = (user_entry *) (((char *) ul2) + cur_len);

	snprintf(&log_msg[0], sizeof(log_msg), "adding user %s", req->user);
	auth_log(log_msg);

	ue->valid = 1;
	strncpy(&ue->name[0], req->user, sizeof(ue->name));
	ue->name[sizeof(ue->name) - 1] = '\0';
	ue->user_gate = req->user_gate;
    }

    if (req->op == auth_dir_lookup) {
	if (!ue_match)
	    throw error(-E_NOT_FOUND, "no such user");

	snprintf(&log_msg[0], sizeof(log_msg), "looking up user %s", req->user);
	auth_log(log_msg);

	reply->user_gate = ue_match->user_gate;
    }
}

static void __attribute__((noreturn))
auth_dir_entry(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    auth_dir_req req = *(auth_dir_req *) &parm->param_buf[0];
    req.user[sizeof(req.user) - 1] = '\0';

    auth_dir_reply reply;
    memset(&reply, 0, sizeof(reply));

    try {
	auth_dir_dispatch(&req, &reply);
    } catch (error &e) {
    	cprintf("auth_dir_entry: %s\n", e.what());
    	reply.err = e.err();
    } catch (std::exception &e) {
    	cprintf("auth_dir_entry: %s\n", e.what());
    	reply.err = -E_INVAL;
    }

    memcpy(&parm->param_buf[0], &reply, sizeof(reply));
    gr->ret(0, 0, 0);
}

static void
auth_dir_init(void)
{
    error_check(auth_dir_grant = handle_alloc());

    label u_ctm(1);
    u_ctm.set(auth_dir_grant, 0);
    u_ctm.set(start_env->process_taint, 3);

    int64_t ct;
    error_check(ct = sys_container_alloc(start_env->shared_container,
					 u_ctm.to_ulabel(), "user list ct",
					 0, CT_QUOTA_INF));

    user_list *ul = 0;
    error_check(segment_alloc(ct, sizeof(*ul), &user_list_seg,
			      (void **) &ul, 0, "user list"));
    scope_guard<int, void *> unmap(segment_unmap, ul);

    label th_ctm, th_clr;
    thread_cur_label(&th_ctm);
    thread_cur_clearance(&th_clr);
    th_ctm.set(start_env->process_grant, 1);

    gate_create(start_env->shared_container,
		"authdir", &th_ctm, &th_clr,
		&auth_dir_entry, 0);
}

int
main(int ac, char **av)
{
    try {
	if (ac != 2) {
	    cprintf("Usage: %s root-grant-handle\n", av[0]);
	    return -1;
	}

	error_check(strtou64(av[1], 0, 10, &root_grant));
    	auth_dir_init();
	process_report_exit(0);
    	thread_halt();
    } catch (std::exception &e) {
    	printf("auth_dir: %s\n", e.what());
    	return -1;
    }
}
