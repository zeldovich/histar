extern "C" {
#include <inc/authd.h>
#include <inc/gateparam.h>

#include <string.h>
}

#include <inc/gateclnt.hh>
#include <inc/authclnt.hh>
#include <inc/error.hh>
#include <inc/labelutil.hh>

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

    req->op = op;
    strncpy(&req->user[0], user, sizeof(req->user));
    strncpy(&req->pass[0], pass, sizeof(req->pass));
    strncpy(&req->npass[0], npass, sizeof(req->npass));
    
    label ds;
    thread_cur_label(&ds);
    // XXX grant all handles?
    gate_call(COBJ(authd_ct, authd_gt), &gcd, 0, &ds, 0);
 
    if (r)
	memcpy(r, reply, sizeof(*r));

    return reply->err;    
}
