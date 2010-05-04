extern "C" {
#include <inc/lib.h>
#include <stdio.h>
#include <inc/syscall.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <inc/error.h>
}
#include <inc/labelutil.hh>
#include <inc/error.hh>

int
main(int ac, char *av[])
{
    if (ac < 2) {
	printf("usage: wattage\n");
	return -1;
    }
    int64_t rate = atol(av[1]);

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

    // do a transfer of rate from root to rs0
    error_check(sys_reserve_transfer(rootrs, rs0, rate, 1));

    // do a transfer of rate from root to rs1
    error_check(sys_reserve_transfer(rootrs, rs1, rate, 1));

    r = sys_reserve_get_level(rs0);
    assert(r == rate);
    r = sys_reserve_get_level(rs1);
    assert(r == rate);

    assert(-E_NO_SPACE == sys_reserve_transfer(rs0, rs1, rate * 3, 1));
    assert(rate == sys_reserve_get_level(rs0));
    assert(rate == sys_reserve_get_level(rs1));

    assert(rate == sys_reserve_transfer(rs0, rs1, rate * 3, 0));
    assert(0 == sys_reserve_get_level(rs0));
    assert(rate * 2 == sys_reserve_get_level(rs1));

    r = sys_reserve_get_level(rootrs);
    sys_self_bill(THREAD_BILL_ENERGY_RAW, 1000000);
    assert(r - sys_reserve_get_level(rootrs) >= 1000000);

    struct cobj_ref myrs;
    sys_self_get_active_reserve(&myrs);
    assert(myrs.object == rootrs.object);
    assert(myrs.container == rootrs.container);

    return 0;
}
