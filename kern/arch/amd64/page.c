#include <machine/pmap.h>
#include <machine/types.h>
#include <dev/kclock.h>
#include <kern/lib.h>
#include <kern/pageinfo.h>
#include <kern/arch.h>
#include <inc/error.h>
#include <inc/queue.h>

// These variables are set by i386_detect_memory()
static physaddr_t maxpa_boot;	// Maximum physical address for boot_alloc
static size_t basemem;		// Amount of base memory (in bytes)
static uint64_t membytes;       // Maximum usuable bytes of the physical AS

// These variables are set in i386_vm_init()
static char *boot_freemem;	// Pointer to next byte of free mem

// Keep track of various page metadata
struct page_info *page_infos;

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
    extern char end[];
    void *v;

    // Initialize boot_freemem if this is the first time.
    // 'end' is a magic symbol automatically generated by the linker,
    // which points to the end of the kernel's bss segment -
    // i.e., the first virtual address that the linker
    // did _not_ assign to any kernel code or global variables.
    if (boot_freemem == 0)
	boot_freemem = end;
    
    boot_freemem = (char *) ROUNDUP (boot_freemem, align);
    if (boot_freemem + n < boot_freemem
	|| boot_freemem + n > (char *) (maxpa_boot + KERNBASE))
	panic ("out of memory during i386_vm_init");
    v = boot_freemem;
    boot_freemem += n;
    return v;
}

static int
nvram_read(int r)
{
    return mc146818_read (r) | (mc146818_read (r + 1) << 8);
}

static void
i386_detect_memory(uint64_t lower_kb, uint64_t upper_kb)
{
    // Worse case, CMOS tells us how many kilobytes there are
    if (!lower_kb)
	lower_kb = nvram_read(NVRAM_BASELO);
    if (!upper_kb)
	upper_kb = nvram_read (NVRAM_EXTLO);

    basemem = ROUNDDOWN(lower_kb * 1024, PGSIZE);
    size_t extmem  = ROUNDDOWN(upper_kb * 1024, PGSIZE);

    // Calculate the maxmium physical address based on whether
    // or not there is any extended memory.  See comment in ../inc/mmu.h.
    if (extmem)
	maxpa_boot = EXTPHYSMEM + extmem;
    else
	maxpa_boot = basemem;

    global_npages = maxpa_boot / PGSIZE;

    cprintf("Physical memory: %dK available, ", (int) (maxpa_boot / 1024));
    cprintf("base = %dK, extended = %dK\n", (int) (basemem / 1024),
	    (int) (extmem / 1024));
}

static void
i386_init(uint64_t lower_kb, uint64_t upper_kb)
{
    i386_detect_memory(lower_kb, upper_kb);

    int inuse;

    // Align boot_freemem to page boundary.
    boot_alloc(0, PGSIZE);

    // Allocate space for page status info.
    uint64_t sz = global_npages * sizeof(*page_infos);
    page_infos = boot_alloc(sz, PGSIZE);
    memset(page_infos, 0, sz);

    // Align to another page boundary.
    boot_alloc(0, PGSIZE);

    for (uint64_t i = 0; i < global_npages; i++) {
	// Off-limits until proven otherwise.
	inuse = 1;

	// The bottom basemem bytes are free except page 0.
	if (i != 0 && i < basemem / PGSIZE)
	    inuse = 0;

	// The IO hole and the kernel abut.

	// The memory past the kernel is free.
	if (i >= RELOC (boot_freemem) / PGSIZE)
	    inuse = 0;

	if (!inuse)
	    page_free(pa2kva(i << PGSHIFT));
    }
}

static void
e820_detect_memory(struct e820entry *desc, uint8_t n)
{
    extern char end[];
    for (uint8_t i = 0; i < n; i++) {
        if (desc[i].type != E820_RAM)
            continue;
        
        membytes += desc[i].size;
        uint64_t s = ROUNDUP(desc[i].addr, PGSIZE);
        uint64_t e = ROUNDDOWN(desc[i].addr + desc[i].size, PGSIZE);

        // boot_alloc can only use memory from the contiguous physical
        // range that holds the kernel symbols.
        if (s < RELOC(end) && RELOC(end) < e)
            maxpa_boot = e;
        
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
   
    // Align boot_freemem to page boundary.
    boot_alloc(0, PGSIZE);

    // Allocate space for page status info.
    uint64_t sz = global_npages * sizeof(*page_infos);
    page_infos = boot_alloc(sz, PGSIZE);
    memset(page_infos, 0, sz);

    // Align to another page boundary.
    boot_alloc(0, PGSIZE);

    struct e820entry *desc = map;
    for (uint8_t i = 0; i < n; i++) {
        if (desc[i].type != E820_RAM)
            continue;
        
        uint64_t s = ROUNDUP(desc[i].addr, PGSIZE);
        uint64_t e = ROUNDDOWN(desc[i].addr + desc[i].size, PGSIZE);

	int inuse;
	for(; s < e; s += PGSIZE) {
	    // Off-limits until proven otherwise.
	    inuse = 1;

	    if (s != 0 && s < IOPHYSMEM)
		inuse = 0;
	    
	    if (s >= RELOC (boot_freemem))
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
    page_alloc_init();

    if (map && n)
	e820_init(map, n);
    else
	i386_init(lower_kb, upper_kb);

    page_stats.pages_used = 0;
}
