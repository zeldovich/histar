#include <assert.h>
#include <login_cap.h>
#include <stdlib.h>
#include <paths.h>
#include <pwd.h>
#include <inc/auth.h>
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/stdio.h>

login_cap_t *
login_getclass(char *class)
{
    static login_cap_t lc;
    return &lc;
}

char *
login_getcapstr(login_cap_t *lc, char *cap, char *def, char *err)
{
    return def;
}

int
login_getcapbool(login_cap_t *lc, char *cap, unsigned int def)
{
    return def;
}

uint64_t
login_getcapnum(login_cap_t *lc, char *cap, uint64_t def, uint64_t err)
{
    return def;
}

uint64_t
login_getcapsize(login_cap_t *lc, char *cap, uint64_t def, uint64_t err)
{
    return def;
}

uint64_t
login_getcaptime(login_cap_t *lc, char *cap, uint64_t def, uint64_t err)
{
    return def;
}

int
setusercontext(login_cap_t *lc, struct passwd *pwd,
	       uid_t uid, unsigned int flags)
{
    if (flags & LOGIN_SETPATH) {
	setenv("PATH", _PATH_STDPATH, 1);
    }

    if (flags & LOGIN_SETRESOURCES) {
	uint64_t ulents[2];
	struct ulabel ul = { .ul_size = 2, .ul_ent = &ulents[0],
			     .ul_nent = 0, .ul_default = 1 };
	assert(0 == label_set_level(&ul, start_env->user_grant, 0, 0));
	assert(0 == label_set_level(&ul, start_env->user_taint, 3, 0));
	int64_t procpool = sys_container_alloc(start_env->shared_container,
					       &ul, "sshd-procpool",
					       0, CT_QUOTA_INF);
	if (procpool < 0)
	    cprintf("procpool alloc: %s\n", e2s(procpool));
	else
	    start_env->process_pool = procpool;
    }

    if (flags & LOGIN_SETUSER) {
	start_env->ruid = pwd->pw_uid;
	start_env->euid = pwd->pw_uid;
    }

    return 0;
}
