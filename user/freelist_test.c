#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/memlayout.h>

struct cobj_ref segs[500] ;

uint64_t 
rand_chunk(void)
{
	return 50 ;
}

// pretty weak...
void
test0()
{
    int r ;
    uint64_t ct = start_env->container;
    void *va = 0;
    
	int i = 0 ;
	for (; i < 10 ; i++) {
		va = 0 ;
		if ((r = segment_alloc(ct, PGSIZE, &segs[i], &va)) < 0)
			panic("error test0 %s", e2s(r)) ;
	}

	for (i = 0 ; i < 10 ; i+=2)
		if ((r = sys_obj_unref(segs[i])) < 0)
			panic("error test0 %s\n", e2s(r)) ;

	for (i = 0 ; i < 10 ; i+=2) {
		va = 0 ;
		if ((r = segment_alloc(ct, PGSIZE, &segs[i], &va)) < 0) 
			panic("error test0 %s\n", e2s(r)) ;
	}
	
	for (i = 0 ; i < 10 ; i++)
		if ((r = sys_obj_unref(segs[i])) < 0)
			panic("error test0 %s\n", e2s(r)) ;
}

int
main(int ac, char **av)
{
	test0() ;
	cprintf("test0 complete\n") ;
}
