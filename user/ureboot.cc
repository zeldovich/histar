extern "C" {
#include <inc/syscall.h>
#include <inc/fs.h>
#include <inc/lib.h>
#include <inc/error.h>

#include <string.h>
#include <stdio.h>
}

#include <inc/error.hh>
#include <inc/spawn.hh>

static void
ureboot_ct(uint64_t ct)
{
    fs_inode init_bin;
    error_check(fs_namei("/bin/init", &init_bin));

    cobj_ref ureboot_self;

    int64_t nslots;
    error_check(nslots = sys_container_get_nslots(ct));
    for (int64_t i = 0; i < nslots; i++) {
	int64_t id = sys_container_get_slot_id(ct, i);
	if (id == -E_NOT_FOUND)
	    continue;
	error_check(id);

	cobj_ref o = COBJ(ct, id);
	int type;
	error_check(type = sys_obj_get_type(o));

	char name[KOBJ_NAME_LEN];
	error_check(sys_obj_get_name(o, &name[0]));
	name[KOBJ_NAME_LEN - 1] = '\0';

	int keep = 0;
	if (type == kobj_container) {
	    if (!strcmp(name, "bin"))
		keep = 1;
	    if (!strcmp(name, "ureboot")) {
		ureboot_self = o;
		keep = 1;
	    }
	}

	if (!keep)
	    error_check(sys_obj_unref(o));
    }

    label ds(3);
    ds.set(start_env->user_grant, LB_LEVEL_STAR);

    label dr(0);
    dr.set(start_env->user_grant, 3);

    child_process cp =
	spawn(start_env->root_container, init_bin,
	      0, 1, 2,
	      0, 0,
	      0, 0,
	      0, &ds, 0, &dr, 0,
	      SPAWN_NO_AUTOGRANT | SPAWN_UINIT_STYLE);

    error_check(sys_obj_unref(ureboot_self));
    sys_self_halt();
}

int
main(int ac, char **av)
{
    int root_anchored = 0;

    if (ac == 2 && !strcmp(av[1], "go-for-it"))
	root_anchored = 1;

    try {
	if (!root_anchored) {
	    fs_inode ureboot;
	    error_check(fs_namei("/bin/ureboot", &ureboot));

	    const char *argv[] = { "ureboot", "go-for-it" };
	    child_process cp =
		spawn(start_env->root_container, ureboot,
		      0, 1, 2,
		      2, &argv[0],
		      0, 0,
		      0, 0, 0, 0, 0);

	    int64_t ec;
	    error_check(process_wait(&cp, &ec));
	}

	ureboot_ct(start_env->root_container);
    } catch (std::exception &e) {
	printf("Error: %s\n", e.what());
    }
}
