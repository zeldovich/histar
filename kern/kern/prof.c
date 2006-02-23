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
struct entry user_table[1];

static struct periodic_task timer ;
static int prof_print_enable = 1;

void 
prof_init(void) 
{
	memset(sysc_table, 0, sizeof(sysc_table)) ;
	memset(trap_table, 0, sizeof(trap_table)) ;
	
	timer.pt_fn = &prof_print ;
	timer.pt_interval_ticks = kclock_hz * 10;
	if (prof_print_enable)
		timer_add_periodic(&timer) ;		
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
prof_user(uint64_t time)
{
	user_table[0].count++;
	user_table[0].time += time;
}

static void
prof_reset(void)
{
	memset(sysc_table, 0, sizeof(sysc_table));
	memset(trap_table, 0, sizeof(trap_table));
	memset(user_table, 0, sizeof(user_table));
}

static void
print_entry(struct entry *tab, int i)
{
	if (tab[i].count)
		cprintf("%3d: count %12ld total %12ld avg %12ld\n",
			i,
			tab[i].count,
			tab[i].time,
			tab[i].time / tab[i].count);
}

void
prof_print(void)
{
	cprintf("prof_print: syscalls\n");
	for (int i = 0 ; i < NSYSCALLS ; i++)
		print_entry(&sysc_table[0], i);

	cprintf("prof_print: traps\n");
	for (int i = 0 ; i < NTRAPS ; i++)
		print_entry(&trap_table[0], i);

	cprintf("prof_print: user\n");
	print_entry(&user_table[0], 0);

	prof_reset();
}
