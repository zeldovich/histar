extern "C" {
#include <inc/authd.h>
#include <inc/gateparam.h>
#include <inc/error.h>
#include <inc/stdio.h>
#include <inc/syscall.h>

#include <string.h>
#include <stdio.h>
}

#include <inc/gateclnt.hh>
#include <inc/authclnt.hh>
#include <inc/error.hh>
#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>

void
auth_login(const char *user, const char *pass, uint64_t *ug, uint64_t *ut)
{
    gate_call_data gcd;
    auth_dir_req      *dir_req      = (auth_dir_req *)      &gcd.param_buf[0];
    auth_dir_reply    *dir_reply    = (auth_dir_reply *)    &gcd.param_buf[0];
    auth_user_req     *user_req     = (auth_user_req *)     &gcd.param_buf[0];
    auth_user_reply   *user_reply   = (auth_user_reply *)   &gcd.param_buf[0];
    auth_uauth_req    *uauth_req    = (auth_uauth_req *)    &gcd.param_buf[0];
    auth_uauth_reply  *uauth_reply  = (auth_uauth_reply *)  &gcd.param_buf[0];
    auth_ugrant_reply *ugrant_reply = (auth_ugrant_reply *) &gcd.param_buf[0];

    int64_t dir_ct, dir_gt;
    error_check(dir_ct = container_find(start_env->root_container, kobj_container, "auth_dir"));
    error_check(dir_gt = container_find(dir_ct, kobj_gate, "authdir"));

    strcpy(&dir_req->user[0], user);
    dir_req->op = auth_dir_lookup;
    gate_call(COBJ(dir_ct, dir_gt), 0, 0, 0).call(&gcd, 0);
    error_check(dir_reply->err);
    cobj_ref user_gate = dir_reply->user_gate;

    // Construct session container, etc.
    int64_t pw_taint, session_grant;
    error_check(pw_taint = handle_alloc());
    scope_guard<void, uint64_t> drop1(thread_drop_star, pw_taint);

    label cur_clear;
    thread_cur_clearance(&cur_clear);
    cur_clear.set(pw_taint, 3);
    thread_set_clearance(&cur_clear);

    error_check(session_grant = handle_alloc());
    scope_guard<void, uint64_t> drop2(thread_drop_star, session_grant);

    label session_label(1);
    session_label.set(session_grant, 0);

    int64_t session_ct;
    error_check(session_ct =
	sys_container_alloc(start_env->shared_container,
			    session_label.to_ulabel(),
			    "login session container", 0, 65536));

    label user_auth_ds(3);
    user_auth_ds.set(session_grant, LB_LEVEL_STAR);

    label user_auth_dr(0);
    user_auth_dr.set(pw_taint, 3);

    user_req->pw_taint = pw_taint;
    user_req->session_ct = session_ct;

    gate_call(user_gate, 0, &user_auth_ds, &user_auth_dr).call(&gcd, 0);
    error_check(user_reply->err);
    cobj_ref uauth_gate  = COBJ(session_ct, user_reply->uauth_gate);
    cobj_ref ugrant_gate = COBJ(session_ct, user_reply->ugrant_gate);
    uint64_t xh = user_reply->xh;

    // Call the user auth gate to check password
    label uauth_taint(LB_LEVEL_STAR);
    uauth_taint.set(pw_taint, 3);

    label cur_label;
    thread_cur_label(&cur_label);

    strcpy(&uauth_req->pass[0], pass);
    uauth_req->change_pw = 0;
    gate_call(uauth_gate, &uauth_taint, 0, 0).call(&gcd, &cur_label);
    int uauth_err = uauth_reply->err;
    memset(&gcd, 0, sizeof(gcd));

    // XXX how can information about the password leak here?
    // -- memset the thread-local segment
    // -- floating-point registers?

    error_check(uauth_err);

    label cur_label2;
    thread_cur_label(&cur_label2);
    cur_label.set(xh, LB_LEVEL_STAR);
    error_check(cur_label2.compare(&cur_label, label::eq));

    // Call the grant gate now..
    gate_call(ugrant_gate, 0, 0, 0).call(&gcd, 0);

    *ug = ugrant_reply->user_grant;
    *ut = ugrant_reply->user_taint;
}

void
auth_chpass(const char *user, const char *pass, const char *npass)
{
    cprintf("auth_chpass not implemented yet\n");
}

void
auth_log(const char *msg)
{
    gate_call_data gcd;
    uint32_t len = MIN(strlen(msg), sizeof(gcd.param_buf));
    memcpy(&gcd.param_buf[0], msg, len);

    int64_t log_ct, log_gt;
    error_check(log_ct = container_find(start_env->root_container, kobj_container, "auth_log"));
    error_check(log_gt = container_find(log_ct, kobj_gate, "authlog"));

    gate_call(COBJ(log_ct, log_gt), 0, 0, 0).call(&gcd, 0);
}
