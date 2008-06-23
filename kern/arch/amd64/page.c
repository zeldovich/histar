#include <machine/pmap.h>
#include <machine/types.h>
#include <dev/kclock.h>
#include <kern/lib.h>
#include <kern/pageinfo.h>
#include <kern/arch.h>
#include <inc/error.h>
#include <inc/queue.h>

// Limit of physical address space
uint64_t pa_limit = UINT64(0x100000000);		

static uint64_t membytes;       // Maximum usuable bytes of the physical AS
static char *boot_freemem;	// Pointer to next byte of free mem
static char *boot_endmem;	// Pointer to first unusable byte

// Keep track of various page metadata
struct page_info *page_infos;

static int
nvram_read(int r)
{
    return mc146818_read (r) | (mc146818_read (r + 1) << 8);
}

//
// Allocate n bytes of physical memory aligned on an 
// align-byte boundary.  Align must be a power of two.
// Return kernel virtual address.  Returned memory is uninitialized.
//
// If we're out of memory, boot_alloc should panic.
// It's too early to run out of memory.
// This function may ONLY be used during initialization,
// before the page_free_list has been set up.
// 
static void *
boot_alloc(uint32_t n, uint32_t align)
{
    boot_freemem = (char *) ROUNDUP (boot_freemem, align);
    if (boot_freemem + n < boot_freemem || boot_freemem + n > boot_endmem)
	panic ("out of memory during i386_vm_init");

    void *v = boot_freemem;
    boot_freemem += n;
    return v;
}

static void
e820_detect_memory(struct e820entry *desc, uint8_t n)
{
    extern char end[];
    for (uint8_t i = 0; i < n; i++) {
	if (desc[i].addr + desc[i].size > pa_limit)
	    pa_limit = desc[i].addr + desc[i].size;

        if (desc[i].type != E820_RAM)
            continue;
        
        membytes += desc[i].size;
        uint64_t s = ROUNDUP(desc[i].addr, PGSIZE);
        uint64_t e = ROUNDDOWN(desc[i].addr + desc[i].size, PGSIZE);

	// boot_alloc can only use memory from the contiguous physical
	// range that holds the kernel symbols.  'end' is a symbol 
	// generated by the linker, which  points to the end of the
	// kernel's bss segment - i.e. the first virtual address that the 
	// linker did _not_ assign to any kernel code or variables.
        if (s < RELOC(end) && RELOC(end) < e) {
	    boot_freemem = (char *)(RELOC(end) + PHYSBASE);
	    boot_endmem = (char *)(e + PHYSBASE);
	}
        
	// global_npages counts from 0 to the last RAM page.
	// The count may include unusable pages!
        if (e / PGSIZE > global_npages)
            global_npages = (e / PGSIZE);
    }
}

static void
e820_init(struct e820entry *map, uint8_t n)
{
    e820_detect_memory(map, n);

    // bootdata.c only maps the first 4 GBs.  Page mappings need to be added
    // to bootpdplo if the physical address space is larger than 4 GBs.
    if (pa_limit > UINT64(0x100000000)) {
	extern struct Pagemap bootpdplo;
	uint64_t gp = ROUNDUP(pa_limit, 0x40000000) / 0x40000000;
	for (uint64_t i = 4; i < gp; i++)
	    bootpdplo.pm_ent[i] = (i * 0x40000000) + (PTE_P|PTE_W|PTE_PS|PTE_G);
    }

    // Align boot_freemem to page boundary.
    boot_alloc(0, PGSIZE);

    // Allocate space for page status info.
    uint64_t sz = global_npages * sizeof(*page_infos);
    page_infos = boot_alloc(sz, PGSIZE);
    memset(page_infos, 0, sz);

    // Align to another page boundary.
    boot_alloc(0, PGSIZE);

    for (uint8_t i = 0; i < n; i++) {
        if (map[i].type != E820_RAM)
            continue;
        
        uint64_t s = ROUNDUP(map[i].addr, PGSIZE);
        uint64_t e = ROUNDDOWN(map[i].addr + map[i].size, PGSIZE);

	int inuse;
	for(; s < e; s += PGSIZE) {
	    // Off-limits until proven otherwise.
	    inuse = 1;

	    if (s != 0 && s < IOPHYSMEM)
		inuse = 0;
	    
	    if (s >= (uint64_t)boot_freemem - PHYSBASE)
		inuse = 0;

	    if (!inuse)
		page_free(pa2kva(s));
	}
    }

    cprintf("Physical memory: %ldMB of %ldMB available\n", 
	    (page_stats.pages_avail << PGSHIFT) / (1024 * 1024),
	    membytes / (1024 * 1024));
}

void
page_init(uint64_t lower_kb, uint64_t upper_kb, struct e820entry *map, uint8_t n)
{
    struct e820entry fm[2];

    page_alloc_init();

    if (!map || !n) {
	// Fake an e820 map
	if (!lower_kb)
	    lower_kb = nvram_read(NVRAM_BASELO);
	if (!upper_kb)
	    upper_kb = nvram_read (NVRAM_EXTLO);
	
	fm[0].addr = 0;
	fm[0].size = ROUNDDOWN(lower_kb * 1024, PGSIZE);
	fm[0].type = E820_RAM;
	assert(fm[0].size <= IOPHYSMEM);
	
	fm[1].addr = EXTPHYSMEM;
	fm[1].size = ROUNDDOWN(upper_kb * 1024, PGSIZE);
	fm[1].type = E820_RAM;
	
	n = 2;
	map = fm;
    }
	
    e820_init(map, n);
    
    page_stats.pages_used = 0;
}
