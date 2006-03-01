#include <machine/trapcodes.h>
#include <lib/hashtable.h>
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
static int prof_print_enable = 0;

static struct periodic_task timer2 ;

// for profiling using gcc's -finstrument-functions
static struct history
{
#define HISTORY_SIZE PGSIZE
        void *func_enter[HISTORY_SIZE] ;
        int enter_size ;

} cyg_history ;

#define NUM_SYMS 800
static struct
{
        int count[NUM_SYMS] ;
        int size ;     
} cyg_data ;

static int cyg_prof_print_enable = 0 ;
static int cyg_prof_enable = 0 ;


static struct hashentry back[NUM_SYMS] ;
static struct hashtable cyg_lookup ;



void __attribute__((no_instrument_function))
prof_init(void) 
{
	memset(sysc_table, 0, sizeof(sysc_table)) ;
	memset(trap_table, 0, sizeof(trap_table)) ;
	
        hash_init(&cyg_lookup, back, NUM_SYMS) ;
        memset(&cyg_data, 0, sizeof(cyg_data)) ;
        
        
	timer.pt_fn = &prof_print ;
	timer.pt_interval_ticks = kclock_hz * 10;
	if (prof_print_enable)
		timer_add_periodic(&timer) ;

        timer2.pt_fn = &cyg_profile_print ;
        timer2.pt_interval_ticks = kclock_hz * 10 ;
        if (cyg_prof_print_enable)
                timer_add_periodic(&timer2) ;	
                
        cyg_prof_enable = 1 ;
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
print_entry(struct entry *tab, int i, const char *name)
{
	if (tab[i].count)
		cprintf("%2d cnt%12ld tot%12ld avg%12ld %s\n",
			i,
			tab[i].count,
			tab[i].time,
			tab[i].time / tab[i].count,
			name);
}

void
prof_print(void)
{
	cprintf("prof_print: syscalls\n");
	for (int i = 0 ; i < NSYSCALLS ; i++)
		print_entry(&sysc_table[0], i, syscall2s(i));

	cprintf("prof_print: traps\n");
	for (int i = 0 ; i < NTRAPS ; i++)
		print_entry(&trap_table[0], i, "trap");

	cprintf("prof_print: user\n");
	print_entry(&user_table[0], 0, "user");

	prof_reset();
}

// XXX
// This doesn't work properly, due to multiple threads and 
// functions that may not return...
static void __attribute__((no_instrument_function))
cyg_profile_kludge(void *func_addr)
{
        uint64_t func = (uint64_t) func_addr ;
        uint64_t val ;
        
        cyg_prof_enable = 0 ;
        if (hash_get(&cyg_lookup, func, &val) < 0) {
                val = cyg_data.size++ ;
                if (hash_put(&cyg_lookup, func, val) < 0)
                        return ;
        }
        cyg_prof_enable = 1 ;
        
        cyg_data.count[val]++ ;
        return ;
}

void __attribute__((no_instrument_function))
__cyg_profile_func_enter(void *this_fn, void *call_site)
{
        if (!cyg_prof_enable)
                return ;
        
        if (!(cyg_history.enter_size < HISTORY_SIZE)) {
                cyg_prof_enable = 0 ;
                cprintf("__cyg_profile_func_enter: no more space\n") ;
                cyg_prof_enable = 1 ;
                return ; 
        }
        
        cyg_history.func_enter[cyg_history.enter_size] = this_fn ;
        cyg_history.enter_size++ ;
}        

void __attribute__((no_instrument_function))
__cyg_profile_func_exit(void *this_fn, void *call_site)
{
        if (!cyg_prof_enable)
                return ;

        // XXX
        if (cyg_history.enter_size == 0)
                return ;
        else if (cyg_history.func_enter[cyg_history.enter_size - 1] != this_fn) {
                cyg_history.enter_size = 0 ;
                return ;
        }
                   
        cyg_history.enter_size-- ;
        cyg_profile_kludge(this_fn) ;
}

// for kobject_alloc lookup
#include <kern/kobj.h>

void __attribute__((no_instrument_function))
cyg_profile_print(void) 
{
        cyg_prof_enable = 0 ;
        
        uint64_t val ;
        if (hash_get(&cyg_lookup, (uint64_t)&kobject_alloc, &val) < 0) {
                cprintf("no hashtable mapping\n") ;
                cyg_prof_enable = 1 ;
                return ;
        }
        cprintf("cyg_data.count[val] %d\n", cyg_data.count[val]) ;
        cyg_prof_enable = 1 ;
}
