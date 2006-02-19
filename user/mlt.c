#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/mlt.h>

int
main(int ac, char **av)
{
    uint64_t ct = start_env->container;

    int64_t mlt_id = sys_mlt_create(ct, 0);
    if (mlt_id < 0)
	panic("mlt_create: %s", e2s(mlt_id));

    struct cobj_ref mlt = COBJ(ct, mlt_id);

    char buf[MLT_BUF_SIZE];
    snprintf(buf, MLT_BUF_SIZE, "%s", "hello world.");

    int r = sys_mlt_put(mlt, 0, &buf[0]);
    if (r < 0)
	panic("mlt_put: %s", e2s(r));

    memset(&buf[0], 0, MLT_BUF_SIZE);
    uint64_t ct_id;
    r = sys_mlt_get(mlt, 0, 0, &buf[0], &ct_id);
    if (r < 0)
	panic("mlt_get: %s", e2s(r));

    printf("mlt data: %s\n", &buf[0]);
    printf("mlt container: %lu\n", ct_id);
    sys_obj_unref(mlt);
}
