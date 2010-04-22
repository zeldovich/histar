extern "C" {
#include <inc/lib.h>
#include <stdio.h>
#include <inc/syscall.h>
#include <unistd.h>
#include <stdlib.h>
}
#include <inc/labelutil.hh>

int
main(int ac, char *av[])
{
    if (ac < 2) {
	printf("usage: wattage\n");
	return -1;
    }
    uint64_t rate = atol(av[1]);

    int64_t rsid = container_find(start_env->root_container, kobj_reserve, "root_reserve");
    if (rsid < 0) {
	perror("couldn't find root_reserve");
	return rsid;
    }
    struct cobj_ref rootrs = COBJ(start_env->root_container, rsid);

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
    r = sys_limit_set_rate(lm1, rate);
    if (r < 0) {
	perror("couldn't set rate on limit1");
	return r;
    }

    for (uint64_t i = 0; i < 10; i++) {
	int64_t level0 = sys_reserve_get_level(rs0);
	printf("rs0 level %lu\n", level0);

	int64_t level1 = sys_reserve_get_level(rs1);
	printf("rs1 level %lu\n", level1);
	sleep(1);
    }

    r = sys_self_set_active_reserve(rs1);
    if (r < 0) {
	perror("couldn't set active reserve");
	return r;
    }

    for (uint64_t i = 0; i < 5; i++) {
	uint64_t start = sys_clock_nsec();
	int64_t level0 = sys_reserve_get_level(rs0);
	// TODO what about errors from get_level?
	printf("rs0 level %ld\n", level0);

	int64_t level1 = sys_reserve_get_level(rs1);
	printf("rs1 level %ld\n", level1);

	uint64_t l = 0;
	for (uint64_t j = 0; j < 1 * 1000 * 1000 * 1000; j++)
	    for (uint64_t k = 0; j < 1 * 1000 * 1000 * 1000; j++)
		l += j * k;
	printf("stuff: %lu\n", l);

	uint64_t end = sys_clock_nsec();
	printf("time to loop: %lu\n", end - start);
    }

    return 0;
}
