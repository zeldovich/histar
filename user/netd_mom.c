#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/assert.h>
#include <inc/fs.h>

static void
netdev_init(uint64_t ct, uint64_t net_grant, uint64_t net_taint)
{
    int64_t netdev_id = container_find(ct, kobj_netdev, 0);
    if (netdev_id < 0) {
	uint64_t net_label[2] = { LB_CODE(net_grant, 0),
				  LB_CODE(net_taint, 3) };
	struct ulabel ul = { .ul_default = 1,
			     .ul_nent = 2,
			     .ul_ent = &net_label[0] };
	netdev_id = sys_net_create(ct, &ul);
	if (netdev_id < 0)
	    panic("cannot create netdev: %s", e2s(netdev_id));
    }
}

int
main(int ac, char **av)
{
    uint64_t rc = start_env->root_container;
    uint64_t ct = start_env->container;

    uint64_t net_grant = sys_handle_create();
    uint64_t net_taint = sys_handle_create();

    netdev_init(rc, net_grant, net_taint);

    struct cobj_ref fsobj;
    int r = fs_lookup(start_env->fs_root, "netd", &fsobj);
    if (r < 0)
	panic("fs_lookup: %s", e2s(r));

    r = spawn(start_env->root_container, fsobj);
    if (r < 0)
	panic("spawn: %s", e2s(r));

    sys_obj_unref(COBJ(rc, ct));
    sys_thread_halt();
}
