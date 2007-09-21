#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/assert.h>

void
netdev_init(void)
{
    int64_t netdev_id;
    uint64_t ct;

    ct = start_env->shared_container;
    netdev_id = container_find(ct, kobj_netdev, 0);
    if (netdev_id < 0) {
	uint64_t ents[8];
	struct ulabel net_label = 
	    { .ul_size = 8, .ul_nent = 0, .ul_ent = ents, .ul_default = 1 };
	label_set_level(&net_label, start_env->process_grant, 0, 0);
	label_set_level(&net_label, start_env->process_taint, 3, 0);
	netdev_id = sys_net_create(ct, 0, &net_label, "jif0");
	if (netdev_id < 0)
	    panic("netdev_init: unable to create netdev: %s", e2s(netdev_id));
    }
}
