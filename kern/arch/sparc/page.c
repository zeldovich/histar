#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/pageinfo.h>

#include <machine/leon.h>
#include <machine/leon3.h>
#include <dev/amba.h>
#include <dev/ambapp.h>

static physaddr_t minpa;	// Minimum physical address
static physaddr_t maxpa;	// Maximum physical address

struct page_info *page_infos;

static char *boot_freemem;	// Pointer to next byte of free mem

static int
esa_mctrl_detect(void)
{
    struct amba_apb_device apb_dev;

    uint32_t r = amba_apbslv_device(VENDOR_ESA, ESA_MCTRL, &apb_dev, 0);
    if (!r)
	return 0;

    uint32_t mcfg2_addr = apb_dev.start + 4;
    uint32_t mcfg2 = lda_bypass(mcfg2_addr);

    if (!(mcfg2 & LEON_MCFG2_SRAMDIS) || !(mcfg2 & LEON_MCFG2_SDRAMEN))
	panic("unexpected memory controller config");
    
    uint32_t sdram_banksz = 
	(mcfg2 & LEON_MCFG2_SDRAMBANKSZ) >> LEON_MCFG2_SDRAMBANKSZ_SHIFT;  

    uint32_t sdram_sz = (1 << sdram_banksz) * 4;

    struct amba_ahb_device ahb_dev;
    r = amba_ahbslv_device(VENDOR_ESA, ESA_MCTRL, &ahb_dev, 0);
    if (!r)
	panic("unable to find memory controller on AHB\n");

    global_npages = sdram_sz << 8;
    minpa = ahb_dev.start[2];
    maxpa = minpa + (global_npages * PGSIZE);

    cprintf("Physical address space layout:\n");
    cprintf(" 0x%08x-0x%08x (ROM)\n", ahb_dev.start[0], ahb_dev.stop[0]);
    cprintf(" 0x%08x-0x%08x (IO)\n", ahb_dev.start[1], ahb_dev.stop[1]);
    cprintf(" 0x%08x-0x%08x (RAM, %dM available)\n", 
	    ahb_dev.start[2], ahb_dev.stop[2], sdram_sz);

    return 1;
}

static int
gaisler_ddrspa_detect(void)
{
    struct amba_ahb_device ahb_dev;
    uint32_t r = amba_ahbslv_device(VENDOR_GAISLER, GAISLER_DDRSPA, &ahb_dev, 0);
    if (!r)
	return 0;
    
    uint32_t sdctrl_addr = ahb_dev.start[1];
    uint32_t sdctrl = lda_bypass(sdctrl_addr);

    uint32_t sdram_banksz = 
	(sdctrl & LEON_SDCTRL_SDRAMBANKSZ) >> LEON_SDCTRL_SDRAMBANKSZ_SHIFT;
    uint32_t sdram_sz = (1 << sdram_banksz) * 8;

    cprintf("SDRAM: 0x%08x-0x%08x (%dM available)\n", 
	    ahb_dev.start[0], ahb_dev.stop[0], sdram_sz);

    global_npages = sdram_sz << 8;
    minpa = ahb_dev.start[0];
    maxpa = minpa + (global_npages * PGSIZE);

    return 1;
}

static void
leon3_detect_memory(void)
{
    if (esa_mctrl_detect())
	return;
    if (gaisler_ddrspa_detect())
	return;
    panic("unable to detect memory controller");
}

static void *
boot_alloc(uint32_t n, uint32_t align)
{
    extern char end[];
    void *v;
    
    if (boot_freemem == 0)
	boot_freemem = end;
    
    boot_freemem = (char *) ROUNDUP (boot_freemem, align);
    if (boot_freemem + n < boot_freemem
	|| boot_freemem + n > (char *) (maxpa + LOAD_OFFSET))
	panic ("boot_alloc: out of memory");
    v = boot_freemem;
    boot_freemem += n;
    return v;
}

void
page_init(void)
{
    page_alloc_init();
    leon3_detect_memory();

    boot_alloc(0, PGSIZE);

    // Allocate space for page status info.
    uint64_t sz = global_npages * sizeof(*page_infos);
    page_infos = boot_alloc(sz, PGSIZE);
    memset(page_infos, 0, sz);

    // Align to another page boundary.
    boot_alloc(0, PGSIZE);

    for (uint32_t pa = (uint32_t) RELOC(boot_freemem); pa < maxpa; pa += PGSIZE)
	page_free(pa2kva(pa));
    page_stats.pages_used = 0;
}
