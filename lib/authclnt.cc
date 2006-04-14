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

int 
auth_call(int op, const char *user, const char *pass, const char *npass,
	  authd_reply *r)
{
    gate_call_data gcd;
    authd_req *req = (authd_req *) &gcd.param_buf[0];
    authd_reply *reply = (authd_reply *) &gcd.param_buf[0];

    int64_t authd_ct = container_find(start_env->root_container,
				      kobj_container, "authd");
    error_check(authd_ct);

    int64_t authd_gt = container_find(authd_ct, kobj_gate, "authd");
    error_check(authd_gt);

    if (op == authd_login || op == authd_chpass)
	req->op = authd_getuser;
    else
	req->op = op;

    strncpy(&req->user[0], user, sizeof(req->user));
    if (op == authd_adduser)
	strncpy(&req->pass[0], pass, sizeof(req->pass));
    
    label verify(3);
    if (start_env->user_grant)
	verify.set(start_env->user_grant, 0);

    gate_call(COBJ(authd_ct, authd_gt), 0, 0, 0).call(&gcd, &verify);

    if (reply->err == 0 && (op == authd_login || op == authd_chpass)) {
	cobj_ref ugate = reply->user_gate;
	uint64_t uid_save = reply->user_id;

	int64_t taint_handle;
	error_check(taint_handle = handle_alloc());
	scope_guard<void, uint64_t> drop_taint(thread_drop_star, taint_handle);

	label cs(LB_LEVEL_STAR);
	label dr(0);

	if (op == authd_login) {
	    cs.set(taint_handle, 3);
	    dr.set(taint_handle, 3);
	}

	req->op = op;
	strncpy(&req->user[0], user, sizeof(req->user));
	strncpy(&req->pass[0], pass, sizeof(req->pass));
	strncpy(&req->npass[0], npass, sizeof(req->npass));

	gate_call(ugate, &cs, 0, &dr).call(&gcd, 0);

	reply->user_id = uid_save;
    }
 
    if (r)
	memcpy(r, reply, sizeof(*r));

    return reply->err;
}
