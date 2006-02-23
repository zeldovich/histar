#include <machine/trapcodes.h>
#include <kern/prof.h>
#include <kern/timer.h>
#include <inc/string.h>

struct entry {
	uint64_t count ;
	uint64_t time ;
} ;

#define NTRAPS (T_SYSCALL + 1)

struct entry sysc_table[NSYSCALLS] ;
struct entry trap_table[NTRAPS] ;

static struct periodic_task timer ;

void 
prof_init(void) 
{
	memset(sysc_table, 0, sizeof(sysc_table)) ;
	memset(trap_table, 0, sizeof(trap_table)) ;
	
	timer.pt_fn = &prof_print ;
	timer.pt_interval_ticks = 1000 ;
#if PROF_PRINT
	timer_add_periodic(&timer) ;		
#endif
}

void 
prof_syscall(syscall_num num, uint64_t time)
{
	assert(num < NSYSCALLS) ;
	sysc_table[num].count++ ;
	sysc_table[num].time += time ;
}

void
prof_trap(int num, uint64_t time)
{
	assert(num < NTRAPS) ;
	trap_table[num].count++ ;
	trap_table[num].time += time ;
}

void
prof_print(void)
{
	for (int i = 0 ; i < NSYSCALLS ; i++) {	
		if (sysc_table[i].count)
			cprintf("%d - %ld %ld\n", i, sysc_table[i].count, sysc_table[i].time) ;	
	}
	
	for (int i = 0 ; i < NTRAPS ; i++) {
		if (trap_table[i].count)
			cprintf("%d - %ld %ld\n", i, trap_table[i].count, trap_table[i].time) ;	
	}
}
