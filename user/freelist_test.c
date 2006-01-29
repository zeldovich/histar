#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/memlayout.h>

#define NSEGS		50
#define SEGPAGES	100

struct cobj_ref segs[NSEGS];

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
    
	int i = 0 ;
	for (; i < NSEGS ; i++)
		if ((r = segment_alloc(ct, SEGPAGES * PGSIZE, &segs[i], 0)) < 0)
			panic("error test0 %s", e2s(r)) ;

	for (i = 0 ; i < NSEGS ; i+=2)
		if ((r = sys_obj_unref(segs[i])) < 0)
			panic("error test0 %s\n", e2s(r)) ;

	for (i = 0 ; i < NSEGS ; i+=2)
		if ((r = segment_alloc(ct, SEGPAGES * PGSIZE, &segs[i], 0)) < 0) 
			panic("error test0 %s\n", e2s(r)) ;
	
/*
	for (i = 0 ; i < NSEGS ; i++)
		if ((r = sys_obj_unref(segs[i])) < 0)
			panic("error test0 %s\n", e2s(r)) ;
*/
}

int
main(int ac, char **av)
{
	test0() ;
	cprintf("test0 complete\n") ;
}
