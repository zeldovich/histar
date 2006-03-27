extern "C" {
#include <inc/authd.h>
#include <inc/gateparam.h>

#include <string.h>
#include <inc/error.h>
}

#include <inc/gateclnt.hh>
#include <inc/authclnt.hh>
#include <inc/error.hh>
#include <inc/labelutil.hh>
#include <stdio.h>

int
auth_groupcall(int op, int type, const char *group, uint64_t t, uint64_t g, authd_reply *r)
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
    strncpy(&req->group[0], group, sizeof(req->group));
    req->taint = t;
    req->grant = g;
    req->type = type;
    
    label ds;
    thread_cur_label(&ds);
 
    // XXX grant all handles?
    gate_call(COBJ(authd_ct, authd_gt), &gcd, 0, &ds, 0, 0);
 
    if (r)
        memcpy(r, reply, sizeof(*r));
 
    return reply->err;       
}
