#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/mlt.h>

int
main(int ac, char **av)
{
    uint64_t ct = start_env->container;

    int64_t mlt_id = sys_mlt_create(ct);
    if (mlt_id < 0)
	panic("mlt_create: %s", e2s(mlt_id));

    struct cobj_ref mlt = COBJ(ct, mlt_id);

    char buf[MLT_BUF_SIZE];
    snprintf(buf, MLT_BUF_SIZE, "%s", "hello world.");

    int r = sys_mlt_put(mlt, &buf[0]);
    if (r < 0)
	panic("mlt_put: %s", e2s(r));

    memset(&buf[0], 0, MLT_BUF_SIZE);
    r = sys_mlt_get(mlt, &buf[0]);
    if (r < 0)
	panic("mlt_get: %s", e2s(r));

    printf("mlt data: %s\n", &buf[0]);
    sys_obj_unref(mlt);
}
