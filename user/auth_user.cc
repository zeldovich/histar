extern "C" {
#include <inc/lib.h>
#include <inc/authd.h>
#include <inc/error.h>
#include <inc/gateparam.h>
#include <inc/string.h>
#include <inc/sha1.h>
#include <inc/syscall.h>
#include <inc/stdio.h>

#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
}

#include <inc/authclnt.hh>
#include <inc/gatesrv.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>
#include <inc/error.hh>
#include <inc/cooperate.hh>

static uint64_t root_grant;
static int64_t user_grant, user_taint;
static cobj_ref user_password_seg;
static cobj_ref base_as_ref;
static char *respect_root;

struct user_password {
    char pwhash[20];
};

struct retry_seg {
    uint64_t xh;
    uint64_t attempts;
};

static void __attribute__((noreturn))
auth_grant_entry(uint64_t arg, gate_call_data *parm, gatesrv_return *gr)
{
    char log_msg[256];
    snprintf(&log_msg[0], sizeof(log_msg), "authenticated OK");
    auth_log(log_msg);

    auth_ugrant_reply *reply = (auth_ugrant_reply *) &parm->param_buf[0];
    reply->user_grant = user_grant;
    reply->user_taint = user_taint;

    label *owner = new label();
    owner->add(user_grant);
    owner->add(user_taint);

    gr->new_ret(owner, 0);
}

static void __attribute__((noreturn))
auth_uauth_entry(uint64_t arg, gate_call_data *parm, gatesrv_return *gr)
{
    auth_uauth_req *req = (auth_uauth_req *) &parm->param_buf[0];
    auth_uauth_reply *reply = (auth_uauth_reply *) &parm->param_buf[0];
    uint64_t retry_seg_id = arg;

    req->pass[sizeof(req->pass) - 1] = '\0';
    req->npass[sizeof(req->npass) - 1] = '\0';

    try {
	struct retry_seg *rs = 0;
	uint64_t nbytes = sizeof(*rs);
	error_check(segment_map(COBJ(req->session_ct, retry_seg_id),
				0, SEGMAP_READ | SEGMAP_WRITE,
				(void **) &rs, &nbytes, 0));
	uint64_t xh = rs->xh;
	if (rs->attempts >= 1)
	    throw basic_exception("too many authentication attempts");
	rs->attempts++;

	struct user_password *pw = 0;
	nbytes = sizeof(*pw);
	error_check(segment_map(user_password_seg, 0, SEGMAP_READ,
				(void **) &pw, &nbytes, 0));
    	scope_guard<int, void *> unmap(segment_unmap, pw);

	sha1_ctx sctx;
	sha1_init(&sctx);
	sha1_update(&sctx, (unsigned char *) req->pass, strlen(req->pass));

	char pwhash[20];
	sha1_final((unsigned char *) &pwhash[0], &sctx);

    	if (memcmp(pwhash, pw->pwhash, sizeof(pw->pwhash))) {
	    label vo, vc;
	    thread_cur_verify(&vo, &vc);

	    if (respect_root[0] != '1' || !vo.contains(root_grant))
		throw error(-E_INVAL, "bad password");
	}

    	if (req->change_pw) {
	    struct user_password *pw2 = 0;
	    error_check(segment_map(user_password_seg, 0, SEGMAP_READ | SEGMAP_WRITE,
				    (void **) &pw2, &nbytes, 0));
	    scope_guard<int, void *> unmap2(segment_unmap, pw2);

	    sha1_init(&sctx);
	    sha1_update(&sctx, (unsigned char *) req->npass, strlen(req->npass));
	    sha1_final((unsigned char *) &pw2->pwhash[0], &sctx);
	}

    	reply->err = 0;
	label *owner = new label();
	owner->add(xh);
    	gr->new_ret(owner, 0);
    } catch (error &e) {
    	cprintf("authd_user_entry: %s\n", e.what());
    	reply->err = e.err();
    } catch (std::exception &e) {
    	cprintf("authd_user_entry: %s\n", e.what());
    	reply->err = -E_INVAL;
    }
    gr->new_ret(0, 0);
}

static void __attribute__((noreturn))
auth_user_entry(uint64_t arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    auth_user_req req = *(auth_user_req *) &parm->param_buf[0];
    auth_user_reply reply;
    memset(&reply, 0, sizeof(reply));

    try {
	reply.ug_cat = user_grant;
	reply.ut_cat = user_taint;

	if (req.req_cats)
	    goto ret;

	char log_msg[256];
	snprintf(&log_msg[0], sizeof(log_msg), "attempting login..");
	auth_log(log_msg);

	int64_t xh;
	error_check(xh = category_alloc(0));

	// OK, creating the retry counter.  First create an empty segment.

	label retry_l;
	retry_l.add(user_grant);
	retry_l.add(user_taint);

	cobj_ref retry_seg;
	struct retry_seg *rs = 0;
	error_check(segment_alloc(req.session_ct, sizeof(*rs),
				  &retry_seg, (void **) &rs,
				  retry_l.to_ulabel(), "retry counter"));
	rs->xh = xh;
	rs->attempts = 0;

	// Now try to copy it with a label of { Pt 3 } using the coop mechanism.
	retry_l.add(req.pw_taint);

	coop_sysarg coop_vals[8];
	memset(&coop_vals[0], 0, sizeof(coop_vals));

	coop_vals[0].u.i = SYS_segment_copy;
	coop_vals[1].u.i = retry_seg.container;
	coop_vals[2].u.i = retry_seg.object;
	coop_vals[3].u.i = req.session_ct;
	coop_vals[4].u.l = &retry_l;
	coop_vals[4].is_label = 1;

	label coop_owner, coop_clear;
	coop_owner.add(user_grant);
	coop_owner.add(user_taint);

	int64_t retry_seg_copy_id;
	error_check(retry_seg_copy_id =
	    coop_gate_invoke(COBJ(req.session_ct, req.coop_gate),
			     &coop_owner, &coop_clear, coop_vals));

	cobj_ref retry_seg_copy = COBJ(req.session_ct, retry_seg_copy_id);

	label retry_l2;
	obj_get_label(retry_seg_copy, &retry_l2);
	if (!retry_l2.contains(user_grant) ||
	    !retry_l2.contains(user_taint) ||
	    !retry_l2.contains(req.pw_taint))
	    throw error(-E_LABEL, "retry segment label mismatch");

	// Whew, created that pesky retry segment..

	label guard;
	guard.add(req.session_grant);

	gatesrv_descriptor gd;
	gd.gate_container_ = req.session_ct;
	gd.name_ = "user auth gate";
	gd.as_ = base_as_ref;
	gd.guard_ = &guard;
	gd.func_ = auth_uauth_entry;
	gd.arg_ = retry_seg_copy.object;
	cobj_ref ga = gate_create(&gd);

	guard.add(xh);
	gd.name_ = "user grant gate";
	gd.func_ = auth_grant_entry;
	gd.arg_ = 0;
	cobj_ref gg = gate_create(&gd);

	reply.err = 0;
	reply.uauth_gate = ga.object;
	reply.ugrant_gate = gg.object;
	reply.xh = xh;
    } catch (error &e) {
    	cprintf("auth_user_entry: %s\n", e.what());
    	reply.err = e.err();
    } catch (std::exception &e) {
    	cprintf("auth_user_entry: %s\n", e.what());
    	reply.err = -E_INVAL;
    }

ret:
    memcpy(&parm->param_buf[0], &reply, sizeof(reply));
    gr->new_ret(0, 0);
}

static void
auth_user_init(void)
{
    /* Don't need any input, close our stdin */
    close(0);
    open("/dev/null", O_RDWR);

    error_check(sys_self_get_as(&base_as_ref));

    int64_t config_ct = 0;
    error_check(config_ct = sys_container_alloc(start_env->shared_container,
						0, "config", 0, 65536));

    // file which controls whether to respect root authority (val: "0" or "1")
    label user_write_protect;
    user_write_protect.add(user_grant);

    cobj_ref respect_root_seg;
    error_check(segment_alloc(config_ct, 2, &respect_root_seg,
			      (void **) &respect_root,
			      user_write_protect.to_ulabel(),
			      "respect-root"));
    respect_root[0] = '1';
    respect_root[1] = '\n';
    error_check(segment_unmap(respect_root));
    error_check(segment_map(respect_root_seg, 0, SEGMAP_READ,
			    (void **) &respect_root, 0, 0));

    // files which export the user's grant and taint categories
    char *s = 0;
    error_check(segment_alloc(config_ct, 128, 0, (void **) &s,
			      0, "user-grant"));
    snprintf(s, 128, "%"PRIu64"\n", user_grant);
    error_check(segment_unmap(s));

    s = 0;
    error_check(segment_alloc(config_ct, 128, 0, (void **) &s,
			      0, "user-taint"));
    snprintf(s, 128, "%"PRIu64"\n", user_taint);
    error_check(segment_unmap(s));

    // password segment
    label pw_ctm;
    pw_ctm.add(user_grant);
    pw_ctm.add(user_taint);

    struct user_password *pw = 0;
    error_check(segment_alloc(config_ct,
			      sizeof(struct user_password),
			      &user_password_seg,
			      (void **)&pw, pw_ctm.to_ulabel(), "password"));
    scope_guard<int, void *> unmap(segment_unmap, pw);

    sha1_ctx sctx;
    sha1_init(&sctx);
    sha1_final((unsigned char *) pw->pwhash, &sctx);

    // any error output
    struct cobj_ref log_seg;
    error_check(segment_alloc(config_ct, 0, &log_seg,
			      0, pw_ctm.to_ulabel(), "log"));

    char logpn[64];
    sprintf(&logpn[0], "#%"PRIu64".%"PRIu64, log_seg.container, log_seg.object);
    int logfd = open(logpn, O_RDWR);
    if (logfd < 0) {
	printf("cannot open log file: %d %s\n", logfd, strerror(errno));
    } else {
	dup2(logfd, 1);
	dup2(logfd, 2);
	close(logfd);
    }

    // gate
    label gt_owner, gt_clear;
    thread_cur_ownership(&gt_owner);
    thread_cur_clearance(&gt_clear);
    gt_owner.remove(start_env->process_grant);

    gate_create(start_env->shared_container,
		"user login gate", &gt_owner, &gt_clear, 0,
		&auth_user_entry, 0);

    error_check(fs_clone_mtab(start_env->shared_container));

    printf("auth_user: ready\n");
}

int
main(int ac, char **av)
{
    try {
	if (ac != 2 && ac != 4) {
	    cprintf("Usage: %s root-grant-handle [user-grant user-taint]\n", av[0]);
	    return -1;
	}

	error_check(strtou64(av[1], 0, 10, &root_grant));

	if (ac == 4) {
	    error_check(strtou64(av[2], 0, 10, (uint64_t *) &user_grant));
	    error_check(strtou64(av[3], 0, 10, (uint64_t *) &user_taint));
	} else {
	    error_check(user_grant = category_alloc(0));
	    error_check(user_taint = category_alloc(1));
	}

    	auth_user_init();
	process_report_exit(0, 0);
	sys_self_halt();
    } catch (std::exception &e) {
    	printf("auth_user: %s\n", e.what());
    	return -1;
    }
}
