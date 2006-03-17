extern "C" {
#include <inc/authd.h>
#include <inc/gateparam.h>

#include <string.h>
}

#include <inc/gateclnt.hh>
#include <inc/authclnt.hh>
#include <inc/error.hh>

void auth_call(int op, const char *user, const char *pass, const char *npass,
	       uint64_t *ut, uint64_t *ug)
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

    gate_call(COBJ(authd_ct, authd_gt), &gcd, 0, 0, 0);

    *ut = reply->user_taint;
    *ug = reply->user_grant;
    error_check(reply->err);    
}
