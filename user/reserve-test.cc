extern "C" {
#include <inc/lib.h>
#include <stdio.h>
#include <inc/syscall.h>
}
#include <inc/labelutil.hh>

int
main(int ac, char *av[])
{
    int64_t rsid = container_find(start_env->root_container, kobj_reserve, "root_reserve");
    if (rsid < 0) {
	perror("couldn't find root_reserve");
	return rsid;
    }
    struct cobj_ref origrsref = COBJ(start_env->root_container, rsid);

    /*
    struct ulabel *ul;
    int64_t r = sys_obj_get_label(origrsref, ul);
    if (r < 0) {
	perror("couldn't get root_reserve label");
	return rsid;
    }
    */

    label l(1);
    //int64_t r = sys_reserve_split(start_env->shared_container, origrsref, l.to_ulabel(), 0, "new_reserve");

    // fork off one reserve
    int64_t r = sys_reserve_split(start_env->process_pool, origrsref, l.to_ulabel(), 0, "reserve0");
    if (r < 0) {
	perror("couldn't split");
	return r;
    }
    printf("New reserve is at %lu\n", r);
    cobj_ref rs0 = COBJ(start_env->process_pool, r);

    // fork off another reserve
    r = sys_reserve_split(start_env->process_pool, origrsref, l.to_ulabel(), 0, "reserve1");
    if (r < 0) {
	perror("couldn't split");
	return r;
    }
    printf("New reserve is at %lu\n", r);
    cobj_ref rs1 = COBJ(start_env->process_pool, r);

    // create a limit between the two reserves
    r = sys_limit_create(start_env->process_pool, rs0, rs1, l.to_ulabel(), "limit0");
    if (r < 0) {
	perror("couldn't create limit");
	return r;
    }
    printf("New limit is at %lu\n", r);
    //cobj_ref lm0 = COBJ(start_env->process_pool, r);

    return 0;
}
