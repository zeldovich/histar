extern "C" {
#include <inc/lib.h>
#include <stdio.h>
#include <inc/syscall.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
}
#include <inc/labelutil.hh>

int
main(int ac, char *av[])
{
    if (ac < 2) {
	printf("usage: wattage leakage_1024_frac\n");
	return -1;
    }
    uint64_t rate = atol(av[1]);
    uint64_t leak = atol(av[2]);
    assert(leak <= 1024);

    int64_t rsid = container_find(start_env->root_container, kobj_reserve, "root_reserve");
    if (rsid < 0) {
	perror("couldn't find root_reserve");
	return rsid;
    }
    struct cobj_ref rootrs = COBJ(start_env->root_container, rsid);

    label l(1);

    // create a reserve
    int64_t r = sys_reserve_create(start_env->process_pool, l.to_ulabel(), "reserve0");
    if (r < 0) {
	perror("couldn't create rs");
	return r;
    }
    printf("New reserve is at %lu\n", r);
    cobj_ref rs0 = COBJ(start_env->process_pool, r);

    // create another reserve
    r = sys_reserve_create(start_env->process_pool, l.to_ulabel(), "reserve1");
    if (r < 0) {
	perror("couldn't create rs");
	return r;
    }
    printf("New reserve is at %lu\n", r);
    cobj_ref rs1 = COBJ(start_env->process_pool, r);

    return 0;
}
