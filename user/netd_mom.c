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
	netdev_id = sys_net_create(ct, &ul, "courtesy of netd_mom");
	if (netdev_id < 0)
	    panic("cannot create netdev: %s", e2s(netdev_id));

	printf("netd_mom: netdev %ld grant %ld taint %ld\n",
	       netdev_id, net_grant, net_taint);
    }
}

int
main(int ac, char **av)
{
    uint64_t rc = start_env->root_container;

    uint64_t net_grant = sys_handle_create();
    uint64_t net_taint = sys_handle_create();

    netdev_init(rc, net_grant, net_taint);

    struct cobj_ref fsobj;
    int r = fs_lookup(start_env->fs_root, "netd", &fsobj);
    if (r < 0)
	panic("fs_lookup: %s", e2s(r));

    struct ulabel *gate_ct_label = label_alloc();
    assert(gate_ct_label);
    gate_ct_label->ul_default = 1;
    assert(0 == label_set_level(gate_ct_label, net_grant, 0, 1));
    int64_t gate_ct = sys_container_alloc(rc, gate_ct_label, "netd gate");
    if (gate_ct < 0)
	panic("netd_mom: creating container for netd gate: %s", e2s(gate_ct));

    struct ulabel *l = label_get_current();
    assert(l);
    label_max_default(l);
    assert(0 == label_set_level(l, net_grant, LB_LEVEL_STAR, 1));
    assert(0 == label_set_level(l, net_taint, LB_LEVEL_STAR, 1));

    printf("netd_mom: spawning netd with label %s\n", label_to_string(l));
    r = spawn_fd(rc, fsobj, 0, 1, 2, 0, 0, l);
    if (r < 0)
	panic("spawn: %s", e2s(r));
}
