#include <machine/pmap.h>
#include <machine/types.h>
#include <machine/boot.h>
#include <machine/memlayout.h>
#include <dev/kclock.h>
#include <kern/lib.h>
#include <kern/pageinfo.h>
#include <kern/arch.h>
#include <inc/error.h>
#include <inc/queue.h>
#include <inc/safeint.h>

// Limit of physical address space
uint64_t pa_limit = UINT64(0x100000000);		

static uint64_t membytes;       // Maximum usuable bytes of the physical AS
static char *boot_freemem;	// Pointer to next byte of free mem
static char *boot_endmem;	// Pointer to first unusable byte

// Keep track of various page metadata
struct page_info *page_infos;

// sorted e820 map
static struct e820entry clean_map[E820MAX * 2];
static uint32_t		clean_n;

// Largest gap below 4G
uint32_t pci_membase;
uint32_t pci_memsize;

static void __attribute__((unused))
e820_print(struct e820entry *desc, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        cprintf(" e820: %016lx - %016lx ", 
                desc[i].addr,
                desc[i].addr + desc[i].size);
        switch (desc[i].type) {
        case E820_RAM:
	    cprintf("(usable)\n");
            break;
        case E820_RESERVED:
            cprintf("(reserved)\n");
            break;
        case E820_ACPI:
            cprintf("(ACPI data)\n");
            break;
        case E820_NVS:
            cprintf("(ACPI NVS)\n");
            break;
	case E820_GAP:
	    cprintf("(gap)\n");
            break;
        default:
            cprintf("type %u\n", desc[i].type);
            break;
        }
    }
}

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
e820_detect_memory(struct e820entry *desc, uint32_t n)
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
	    int of = 0;
	    boot_freemem = end;
	    boot_endmem = (char *) (safe_addptr(&of, e, KERNBASE));
	    if (of)
		boot_endmem = (char *) (uintptr_t) UINT64(~0);
	}
        
	// global_npages counts from 0 to the last RAM page.
	// The count may include unusable pages!
        if (e / PGSIZE > global_npages)
            global_npages = (e / PGSIZE);
    }
}

static void
e820_init(struct e820entry *map, uint32_t n)
{
    e820_detect_memory(map, n);

#ifdef JOS_ARCH_amd64
    // bootdata.c only maps the first 4 GBs.  Page mappings need to be added
    // to bootpdplo if the physical address space is larger than 4 GBs.
    if (pa_limit > UINT64(0x100000000)) {
	extern struct Pagemap bootpdplo;
	uint64_t gp = ROUNDUP(pa_limit, 0x40000000) / 0x40000000;
	for (uint64_t i = 4; i < gp; i++)
	    bootpdplo.pm_ent[i] = (i * 0x40000000) + (PTE_P|PTE_W|PTE_PS|PTE_G);
    }
#endif

    // Align boot_freemem to page boundary.
    boot_alloc(0, PGSIZE);

    // Allocate space for page status info.
    uint64_t sz = global_npages * sizeof(*page_infos);
    page_infos = boot_alloc(sz, PGSIZE);
    memset(page_infos, 0, sz);

    // Align to another page boundary.
    boot_alloc(0, PGSIZE);

    for (uint32_t i = 0; i < n; i++) {
        uint64_t s = ROUNDUP(map[i].addr, PGSIZE);
        uint64_t e = ROUNDDOWN(map[i].addr + map[i].size, PGSIZE);

	// We only have global_npages page_infos.
	if (e > ppn2pa(global_npages))
	    break;

	// We need to "reserve" all non-E820_RAM pages < global_npages.
        if (map[i].type != E820_RAM) {
	    for(; s < e; s += PGSIZE)
		page_reserve(pa2kva(s));
            continue;
	}
        
	int inuse;
	for(; s < e; s += PGSIZE) {
	    // Off-limits until proven otherwise.
	    inuse = 1;

	    if (s != 0 && s != APBOOTSTRAP && s < IOPHYSMEM)
		inuse = 0;
	    
	    if (s >= RELOC (boot_freemem))
		inuse = 0;

	    if (!inuse)
		page_free(pa2kva(s));
	}
    }

    cprintf("Physical memory: %"PRIu64"MB of %"PRIu64"MB available\n", 
	    (page_stats.pages_avail << PGSHIFT) / (1024 * 1024),
	    membytes / (1024 * 1024));
}

static void
e820_sanitize(struct e820entry *map, uint32_t n)
{
    // Insertion sort the e820 map.  We assume the BIOS e820 map 
    // has non-overlapping entries.
    for (uint32_t i = 0; i < n; i++) {    
	uint32_t k;
	struct e820entry *e = &map[i];

	for (k = 0; k < i; k++)
	    if (e->addr < clean_map[k].addr)
		break;

	memmove(&clean_map[k + 1], &clean_map[k], 
		(i - k) * sizeof(struct e820entry));
	clean_map[k] = *e;
    }

    clean_n = n;

    // Mark all the gaps in the e820 map
    for (uint32_t i = 0; i < clean_n - 1; i++) {
	if (clean_map[i].addr + clean_map[i].size != clean_map[i + 1].addr) {
	    uint64_t addr, size;

	    addr = clean_map[i].addr + clean_map[i].size;
	    size = clean_map[i + 1].addr - addr;

	    memmove(&clean_map[i + 2], &clean_map[i + 1], 
		    (clean_n - i - 1) * sizeof(struct e820entry));
	    clean_map[i + 1].addr = addr;
	    clean_map[i + 1].size = size;
	    clean_map[i + 1].type = E820_GAP;

	    // Remember the largest gap below 4GB for PCI MMIO
	    if (size > pci_memsize && addr + size < 0x100000000) {
		pci_membase = addr;
		pci_memsize = size;
	    }

	    clean_n++;
	    i++;
	}
    }
}

void
page_init(uint64_t lower_kb, uint64_t upper_kb, struct e820entry *map, uint32_t n)
{
    struct e820entry fm[2];

    page_alloc_init();

    if (!map || !n) {
	// Fake an e820 map
	if (!lower_kb)
	    lower_kb = nvram_read(NVRAM_BASELO);
	if (!upper_kb)
	    upper_kb = nvram_read(NVRAM_EXTLO);
	
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

    e820_sanitize(map, n);
    //e820_print(map, n);
    //e820_print(clean_map, clean_n);
    e820_init(clean_map, clean_n);
    page_stats.pages_used = 0;
}
