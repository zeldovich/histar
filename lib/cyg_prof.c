#include <inc/stdio.h>
#include <inc/hashtable.h>
#include <inc/assert.h>
#include <inc/lib.h>

#include <machine/x86.h>
#include <machine/memlayout.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

void __attribute__ ((no_instrument_function))
     __cyg_profile_func_enter(void *this_fn, void *call_site);
void __attribute__ ((no_instrument_function))
     __cyg_profile_func_exit(void *this_fn, void *call_site);
void cyg_profile_init(void);


struct entry {
    uint64_t count;
    uint64_t time;
};

struct func_stamp {
    uint64_t func_addr;
    uint64_t entry_tsc;
};

struct cyg_stack {
    uint64_t tid;

    int size;
    struct func_stamp func_stamp[PGSIZE];
};

static struct {
    char enable;	// to avoid re-entry while profiling

#define NUM_STACKS 16
    struct cyg_stack stack[NUM_STACKS];

#define NUM_SYMS 800
    // map from func ptr to index into stats table
    struct hashtable stats_lookup;
    struct hashentry stats_lookup_back[NUM_SYMS];

    int stat_size;
    struct entry stat[NUM_SYMS];

    uint64_t last_tsc;
} cyg_data;

enum { prof_print_count_threshold = 0 };
enum { prof_print_cycles_threshold = UINT64(0) };

static uint64_t cyg_profs_threshold = UINT64(0);

#define NUM_PROFS_PRINTED (sizeof(cyg_profs_printed) / sizeof(cyg_profs_printed[0]))

static void __attribute__ ((no_instrument_function))
cyg_profile_reset(void)
{
    memset(cyg_data.stat, 0, sizeof(cyg_data.stat));
}

static void __attribute__ ((no_instrument_function))
print_entry(struct entry *tab, int i, const char *name)
{
    if (tab[i].count > prof_print_count_threshold ||
	tab[i].time > prof_print_cycles_threshold)
	cprintf("%3d cnt%12"PRIu64" tot%12"PRIu64" avg%12"PRIu64" %s\n",
		i,
		tab[i].count, tab[i].time, tab[i].time / tab[i].count, name);
}

static struct cyg_stack * __attribute__ ((no_instrument_function))
stack_for(uint64_t tid)
{
    for (int i = 0; i < NUM_STACKS; i++) {
	if (cyg_data.stack[i].tid == tid)
	    return &cyg_data.stack[i];
    }
    return 0;
}

static struct cyg_stack * __attribute__ ((no_instrument_function))
stack_alloc(uint64_t tid)
{
    for (int i = 0; i < NUM_STACKS; i++) {
	if (cyg_data.stack[i].size == 0) {
	    cyg_data.stack[i].tid = tid;
	    return &cyg_data.stack[i];
	}
    }
    return 0;
}

static void __attribute__ ((no_instrument_function))
cyg_profile_print(void)
{
    if (!cyg_data.enable)
	return;

    cyg_data.enable = 0;

    cprintf("cyg_profile_print: over-threshold functions\n");
    for (int i = 0; i < NUM_SYMS; i++) {
	char buf[32];
	uint64_t key = cyg_data.stats_lookup_back[i].key;
	uint64_t val = cyg_data.stats_lookup_back[i].val;
	if (!key)
	    continue;

	sprintf(buf, "%"PRIx64, key);
	if (cyg_data.stat[val].time > cyg_profs_threshold)
	    print_entry(cyg_data.stat, val, buf);
    }

    cyg_profile_reset();
    cyg_data.enable = 1;
}

static void __attribute__ ((no_instrument_function))
cyg_profile_data(void *func_addr, uint64_t tm, int count)
{
    uint64_t func = (uintptr_t) func_addr;
    uint64_t val;

    if (hash_get(&cyg_data.stats_lookup, func, &val) < 0) {
	val = cyg_data.stat_size++;
	if (hash_put(&cyg_data.stats_lookup, func, val) < 0)
	    return;
    }

    cyg_data.stat[val].count += count;
    cyg_data.stat[val].time += tm;

    return;
}

void __attribute__ ((no_instrument_function))
__cyg_profile_func_enter(void *this_fn, void *call_site)
{
    if (!cyg_data.enable)
	return;

    cyg_data.enable = 0;

    struct cyg_stack *s;
    if ((s = stack_for(thread_id())) == 0)
	assert((s = stack_alloc(thread_id())) != 0);
    
    //struct cyg_stack *s = &cyg_data.stack[0];

    uint64_t f = read_tsc();
    if (s->size > 0) {
	uint64_t caller = s->func_stamp[s->size - 1].func_addr;
	cyg_profile_data((void *) (uintptr_t) caller, f - cyg_data.last_tsc, 0);
    }
    cyg_data.last_tsc = f;

    s->func_stamp[s->size].func_addr = (uintptr_t) this_fn;
    s->func_stamp[s->size].entry_tsc = read_tsc();
    s->size++;

    // overflow func addr stack
    assert(s->size != PGSIZE);

    cyg_data.enable = 1;

}

void __attribute__ ((no_instrument_function))
__cyg_profile_func_exit(void *this_fn, void *call_site)
{
    if (!cyg_data.enable)
	return;

    cyg_data.enable = 0;
    uint64_t f = read_tsc();

    //struct cyg_stack *s = &cyg_data.stack[0];
    struct cyg_stack *s = stack_for(thread_id());
    if (!s)
	return;
    
    while (1) {
	s->size--;
	if (s->func_stamp[s->size].func_addr == (uintptr_t) this_fn)
	    break;
	// bottom out func addr stack
	assert(s->size != 0);
    }

    uint64_t tm = f - cyg_data.last_tsc;
    cyg_data.last_tsc = f;
    cyg_profile_data(this_fn, tm, 1);

    // To account for time spent in a function including all of the
    // children called from there, use this:
    //uint64_t time_with_children = f - s->func_stamp[s->size].entry_tsc;

    cyg_data.enable = 1;

}

void
cyg_profile_init(void)
{
    hash_init(&cyg_data.stats_lookup, cyg_data.stats_lookup_back, NUM_SYMS);
    atexit(cyg_profile_print);
    cyg_data.enable = 1;
}
