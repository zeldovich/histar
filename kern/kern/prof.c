#include <machine/trapcodes.h>
#include <machine/x86.h>
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
static int cyg_prof_print_enable = 0 ;
static int cyg_prof_enable = 0 ;

struct func_stamp
{
        uint64_t func_addr ;
        uint64_t tsc ;        
} ;

struct cyg_stack
{
        uint64_t stack_base ;
        
        int size ;     
        //uint64_t func_addr[PGSIZE] ;       
        struct func_stamp func_stamp[PGSIZE] ;       
} ;

struct cyg_stats
{
        uint64_t count ;       
        uint64_t time ;
} ;

static struct
{
#define NUM_STACKS 16
        // map from sp base to stack holding enters
        struct cyg_stack stack[NUM_STACKS] ;

#define NUM_SYMS 800
        // map from func ptr to index into stats table
        struct hashtable stats_lookup ;
        struct hashentry stats_lookup_back[NUM_SYMS] ;
     
        int stat_size ;
        struct cyg_stats stat[NUM_SYMS] ;
        
} cyg_data ;


void __attribute__((no_instrument_function))
prof_init(void) 
{
	memset(sysc_table, 0, sizeof(sysc_table)) ;
	memset(trap_table, 0, sizeof(trap_table)) ;
	
        memset(&cyg_data, 0, sizeof(cyg_data)) ;
        hash_init(&cyg_data.stats_lookup, cyg_data.stats_lookup_back, NUM_SYMS) ;
        
        
	timer.pt_fn = &prof_print ;
	timer.pt_interval_ticks = kclock_hz * 10;
	if (prof_print_enable)
		timer_add_periodic(&timer) ;

        timer2.pt_fn = &cyg_profile_print ;
        timer2.pt_interval_ticks = kclock_hz * 10 ;
        if (cyg_prof_print_enable)
                timer_add_periodic(&timer2) ;	
                
        cyg_prof_enable = 0 ;
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
static void __attribute__((no_instrument_function))
cyg_profile_func(void *func_addr, uint64_t time)
{
        uint64_t func = (uint64_t) func_addr ;
        uint64_t val ;
        
        cyg_prof_enable = 0 ;
        if (hash_get(&cyg_data.stats_lookup, func, &val) < 0) {
                val = cyg_data.stat_size++;
                if (hash_put(&cyg_data.stats_lookup, func, val) < 0)
                        return ;
        }
        cyg_prof_enable = 1 ;
        
        cyg_data.stat[val].count++ ;
        cyg_data.stat[val].time += time ;
        
        return ;
}

static void __attribute__((no_instrument_function))
cyg_profile_dump(void)
{
        for (int i = 0 ; i < NUM_STACKS ; i++) {              
                cprintf(" base %lx\n", cyg_data.stack[i].stack_base) ;      
                cprintf("  top %lx\n", cyg_data.stack[i].func_stamp[cyg_data.stack[i].size - 1].func_addr) ;       
        }
}

void __attribute__((no_instrument_function))
__cyg_profile_func_enter(void *this_fn, void *call_site)
{
        if (!cyg_prof_enable)
                return ;
        
        cyg_prof_enable = 0 ;
        uint64_t sp = ROUNDUP(read_rsp(), PGSIZE) ;
        int i = 0 ;
        for (; i < NUM_STACKS ; i++) {
                if (cyg_data.stack[i].stack_base == sp)
                        break ;                               
        }
        
        if (i == NUM_STACKS) {
                for (i = 0 ; i < NUM_STACKS ; i++) {       
                        if (cyg_data.stack[i].stack_base == 0) {
                                cyg_data.stack[i].stack_base = sp ;
                                break ;       
                        }
                }
        }

        if (i == NUM_STACKS) {
                cyg_profile_dump() ;
                panic("__cyg_profile_func_enter: out of func addr stacks") ;
        }

        cyg_data.stack[i].func_stamp[cyg_data.stack[i].size].func_addr = (uint64_t) this_fn ;
        cyg_data.stack[i].func_stamp[cyg_data.stack[i].size].tsc = read_tsc() ;
        cyg_data.stack[i].size++ ;
        
        if (cyg_data.stack[i].size == PGSIZE)
                panic("__cyg_profile_func_enter: overflow func addr stack") ;
        
        cyg_prof_enable = 1 ;
}        

void __attribute__((no_instrument_function))
__cyg_profile_func_exit(void *this_fn, void *call_site)
{
        if (!cyg_prof_enable)
                return ;

        cyg_prof_enable = 0 ;
        uint64_t sp = ROUNDUP(read_rsp(), PGSIZE) ;
        int i = 0 ;
        for (; i < NUM_STACKS ; i++) {
                if (cyg_data.stack[i].stack_base == sp)
                        break ;                               
        }

        assert(i < NUM_STACKS) ;

        while (1) {
                cyg_data.stack[i].size-- ;
                if (cyg_data.stack[i].func_stamp[cyg_data.stack[i].size].func_addr == (uint64_t)this_fn)
                        break ;
                        
                if (cyg_data.stack[i].size == 0)
                        panic("__cyg_profile_func_exit: func addr stack bottomed out") ;
        }
       
        if (cyg_data.stack[i].size == 0)
                cyg_data.stack[i].stack_base = 0 ;


        uint64_t time = read_tsc() - cyg_data.stack[i].func_stamp[cyg_data.stack[i].size].tsc ;
        cyg_profile_func(this_fn, time) ;
}

void __attribute__ ((no_instrument_function))
cyg_profile_free_stack(uint64_t sp)
{
        if (!cyg_prof_enable)
                return ;
        
        sp = ROUNDUP(sp, PGSIZE) ;
        
        for (int i = 0 ; i < NUM_STACKS ; i++) {
                if (cyg_data.stack[i].stack_base == sp) {
                        cyg_data.stack[i].stack_base = 0 ;                               
                        cyg_data.stack[i].size = 0 ;
                        return ;
                }
        }        
}

void __attribute__((no_instrument_function))
cyg_profile_print(void) 
{
        if (!cyg_prof_enable)
                return ;
                
        cyg_prof_enable = 0 ;
        /*
        uint64_t func = (uint64_t) &pstate_test ;
        uint64_t val ;
        
        if (hash_get(&cyg_data.stats_lookup, func, &val) == 0) {
                cprintf("profile: %d\n", cyg_data.stat[val].count) ;
        }
        else {
                cprintf("profile: no hashtable entry\n") ;   
        }
        */
        cyg_prof_enable = 1 ;
}
