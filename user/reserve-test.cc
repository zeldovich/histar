extern "C" {
#include <inc/lib.h>
#include <stdio.h>
#include <inc/syscall.h>
#include <unistd.h>
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
    struct cobj_ref rootrs = COBJ(start_env->root_container, rsid);

    /*
    struct ulabel *ul;
    int64_t r = sys_obj_get_label(origrsref, ul);
    if (r < 0) {
	perror("couldn't get root_reserve label");
	return rsid;
    }
    */

    label l(1);

    // fork off one reserve
    int64_t r = sys_reserve_split(start_env->process_pool, rootrs, l.to_ulabel(), 0, "reserve0");
    if (r < 0) {
	perror("couldn't split");
	return r;
    }
    printf("New reserve is at %lu\n", r);
    cobj_ref rs0 = COBJ(start_env->process_pool, r);

    // fork off another reserve
    r = sys_reserve_split(start_env->process_pool, rootrs, l.to_ulabel(), 0, "reserve1");
    if (r < 0) {
	perror("couldn't split");
	return r;
    }
    printf("New reserve is at %lu\n", r);
    cobj_ref rs1 = COBJ(start_env->process_pool, r);

    // create a limit between the root and rs0
    r = sys_limit_create(start_env->process_pool, rootrs, rs0, l.to_ulabel(), "limit0");
    if (r < 0) {
	perror("couldn't create limit");
	return r;
    }
    printf("New limit is at %lu\n", r);
    cobj_ref lm0 = COBJ(start_env->process_pool, r);
    r = sys_limit_set_rate(lm0, 1000);
    if (r < 0) {
	perror("couldn't set rate on limit0");
	return r;
    }

    // create a limit between the two reserves
    r = sys_limit_create(start_env->process_pool, rs0, rs1, l.to_ulabel(), "limit1");
    if (r < 0) {
	perror("couldn't create limit");
	return r;
    }
    printf("New limit is at %lu\n", r);
    cobj_ref lm1 = COBJ(start_env->process_pool, r);
    r = sys_limit_set_rate(lm1, 100);
    if (r < 0) {
	perror("couldn't set rate on limit1");
	return r;
    }

    for (uint64_t i = 0; i < 5; i++) {
	int64_t level0 = sys_reserve_get_level(rs0);
	if (r < 0) {
	    perror("couldn't get level on reserve");
	    return r;
	}
	printf("rs0 level %lu\n", level0);

	int64_t level1 = sys_reserve_get_level(rs1);
	if (r < 0) {
	    perror("couldn't get level on reserve");
	    return r;
	}
	printf("rs1 level %lu\n", level1);
	sleep(1);
    }

    return 0;
}
