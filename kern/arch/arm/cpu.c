#include <kern/arch.h>
#include <kern/lib.h>
#include <dev/goldfish_irq.h>
#include <machine/arm.h>
#include <machine/asm.h>
#include <machine/atag.h>
#include <machine/pmap.h>
#include <machine/cpu.h>

// cache globals 
uint32_t arm_pcache_vipt = 0;
uint32_t arm_pcache_aliasing = 0;

uint32_t arm_picache_ways = 0;
uint32_t arm_picache_sets = 0;
uint32_t arm_picache_line_size = 0;
uint32_t arm_picache_size = 0;
uint32_t arm_picache_pbit = 0;

uint32_t arm_pdcache_ways = 0;
uint32_t arm_pdcache_sets = 0;
uint32_t arm_pdcache_line_size = 0;
uint32_t arm_pdcache_size = 0;
uint32_t arm_pdcache_pbit = 0;

uint32_t arm_pcache_unified = 0;

// cpu-specific functions
struct cpufunc cpufunc;

enum cpu_class {
	CLASS_ARM9EJS = 1,
	CLASS_ARM11J
};

struct cpuidtbl {
	uint32_t	id;
	enum cpu_class	class;
	const char     *name;
};

//inspired by NetBSD
const struct cpuidtbl cpuids[] = {
	{ 0x41069260,	CLASS_ARM9EJS,	"ARM926EJ-S" },
	{ 0x4107b360,	CLASS_ARM11J,	"ARM1136JS" },
	{ 0x4117b360,	CLASS_ARM11J,	"ARM1136JSR1" },
	{ 0, 0, NULL }
};

enum cache_type {
	UNIFIED_CACHE,
	I_CACHE,
	D_CACHE
};

struct cachetbl {
	uint32_t    ctype;
	const char *name;
};

const struct cachetbl cachetypes[] = {
	{ 0x0, "write-through" },
	{ 0x1, "write-back (read flush, no lock-down)" },
	{ 0x2, "write-back (no lock-down)" },
	{ 0x6, "write-back (lock-down format A)" },
	{ 0x7, "write-back (lock-down format B)" },
	{ 0xe, "write-back (lock-down format C)" },
	{ 0x5, "write-back (lock-down format D)" }
};

const int cachesizes[2][16] = {
	{ 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072 },
	{ 768, 1536, 3072, 6144, 12288, 24576, 49152, 98304, 196608 }
};

const int cacheassocs[2][8] = {
	{ 1, 2, 4, 8, 16, 32, 64, 128 },
	{ 0, 3, 6, 12, 24, 48, 96, 192 }
};

const int cachelinelens[4] = { 8, 16, 32, 64 };

static void cpu_noop(void) { }

static void
identify_cpu()
{
	const char *name = "<unknown>";
	enum cpu_class class = 0;
	uint32_t id = cp15_main_id_get();
	int i;

	for (i = 0; cpuids[i].name != NULL; i++) { 
		if (cpuids[i].id == (id & 0xfffffff0)) {
			name  = cpuids[i].name;
			class = cpuids[i].class;
			break;
		}
	}

	cprintf("CPU: %s revision 0x%x (0x%08x)\n", name, id & 0xf, id);

	switch (class) {
	case CLASS_ARM11J:
		cpufunc.cf_tlb_flush_entry = cp15_tlb_flush_entry_arm11;
		cpufunc.cf_write_buffer_drain = cp15_write_buffer_drain;
		cpufunc.cf_icache_invalidate = cp15_icache_invalidate_arm11;
		cpufunc.cf_dcache_flush_invalidate =
		    cp15_dcache_flush_invalidate_arm11;
		cpufunc.cf_dcache_flush_invalidate_range =
		    cp15_dcache_flush_invalidate_range_arm11;
		cpufunc.cf_sleep = cp15_wait_for_interrupt_arm1136;
		break;

	case CLASS_ARM9EJS:
		/* XXX */
		cpufunc.cf_tlb_flush_entry = (void *)cp15_tlb_flush;
		cpufunc.cf_write_buffer_drain = cp15_write_buffer_drain;
		cpufunc.cf_icache_invalidate = (void *)cpu_noop;
		cpufunc.cf_dcache_flush_invalidate = (void *)cpu_noop;
		cpufunc.cf_dcache_flush_invalidate_range = (void *)cpu_noop;
		cpufunc.cf_sleep = (void *)cpu_noop;
		break;

	default:
		panic("unsupported cpu");
	}
}

static void
identify_cache_type(enum cache_type type, uint32_t bits)
{
	int pbit   = (bits & 0x800) != 0;
	int total  = cachesizes [(bits >> 2) & 0x1][(bits >> 6) & 0xf]; 
	int assoc  = cacheassocs[(bits >> 2) & 0x1][(bits >> 3) & 0x7];
	int linesz = cachelinelens[bits & 0x3];
	int nsets;

	if (assoc == 0) {
		cprintf("CPU: cache absent\n");
		return;
	}

	nsets = (total / assoc / linesz);

	if (type == UNIFIED_CACHE) {
		cprintf("CPU: unified I/D-cache ");
		arm_pcache_unified    = 1;
		arm_picache_ways      = arm_pdcache_ways      = assoc;
		arm_picache_sets      = arm_pdcache_sets      = nsets;
		arm_picache_line_size = arm_pdcache_line_size = linesz;
		arm_picache_size      = arm_pdcache_size      = total;
		arm_picache_pbit      = arm_pdcache_pbit      = pbit;
	} else if (type == I_CACHE) {
		cprintf("CPU: I-cache ");
		arm_picache_ways      = assoc;
		arm_picache_sets      = nsets;
		arm_picache_line_size = linesz;
		arm_picache_size      = total;
		arm_picache_pbit      = pbit;
	} else {
		assert(type == D_CACHE);
		cprintf("CPU: D-cache ");
		arm_pdcache_ways      = assoc;
		arm_pdcache_sets      = nsets;
		arm_pdcache_line_size = linesz;
		arm_pdcache_size      = total;
		arm_pdcache_pbit      = pbit;
	}

	if (total < 1024)
		cprintf("%d bytes ", total);
	else if (total % 1024)
		cprintf("%d.%dKB ", total / 1024, (total % 1024) / 100);
	else
		cprintf("%dKB ", total / 1024);

	cprintf("(%d sets, %d-way %s, %d bytes/line)%s\n",
	    nsets, assoc, (assoc == 1) ? "direct-mapped" : "associative",
	    linesz, (pbit) ? ", P-bit" : "");
}

static void
identify_cache()
{
	const char *name = "<unknown>";
	uint32_t cache = cp15_cache_type_get();
	uint32_t ctype, dsize, isize;
	int unified;
	int v7type, i;

	if (cache == cp15_main_id_get()) {
		// XXX- what to do? assume vivt? assume no cache?
		panic("%s:%s: cache type register unimplemented\n",
		   __FILE__, __func__);
	}

	v7type  = ((cache & 0xe0000000) == 0x80000000);
	ctype   = (cache >> 25) & 0xf;
	unified = (cache & 0x1000000) == 0;
	dsize   = (cache >> 12) & 0xfff;
	isize   = (cache >>  0) & 0xfff;

	if (v7type) {
		arm_pcache_vipt = 1;
		arm_pcache_aliasing = 0;
	} else {
		arm_pcache_vipt = ((cache & 0x1e000000) == 0x1c000000);
		arm_pcache_aliasing = ((cache & 0x1e800000) == 0x1e800000);
	}

	for (i = 0; cachetypes[i].name != NULL; i++) {
		if (cachetypes[i].ctype == ctype) {
			name = cachetypes[i].name;
			break;
		}
	}

	cprintf("CPU: %s %saliasing cache: %s (0x%08x)\n",
	    (arm_pcache_vipt) ? "VIPT" : "VIVT",
	    (arm_pcache_aliasing) ? "non-" : "", name, cache);

	if (unified) {
		if (isize != dsize)
			cprintf("CPU: WARNING: unified cache isize != dsize\n");
		identify_cache_type(UNIFIED_CACHE, dsize);
	} else {
		identify_cache_type(D_CACHE, dsize);
		identify_cache_type(I_CACHE, isize);
	}
}

void
cpu_identify()
{
	identify_cpu();
	identify_cache();
}
