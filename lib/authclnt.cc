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
    cprintf("auth_login not implemented yet\n");
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
