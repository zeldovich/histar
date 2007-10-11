#include <assert.h>
#include <inc/auth.h>
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/stdio.h>

int
jos64_session_setup(void)
{
    uint64_t ulents[2];
    struct ulabel ul = { .ul_size = 2, .ul_ent = &ulents[0],
			 .ul_nent = 0, .ul_default = 1 };
    assert(0 == label_set_level(&ul, start_env->user_grant, 0, 0));
    assert(0 == label_set_level(&ul, start_env->user_taint, 3, 0));
    int64_t procpool = sys_container_alloc(start_env->shared_container,
					   &ul, "sshd-procpool",
					   0, CT_QUOTA_INF);
    if (procpool < 0) {
	cprintf("procpool alloc: %s\n", e2s(procpool));
	return procpool;
    }

    start_env->process_pool = procpool;
    return 0;
}
