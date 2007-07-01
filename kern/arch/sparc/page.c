#include <kern/lib.h>
#include <kern/arch.h>

#include <machine/ambapp.h>
#include <machine/leon.h>
#include <machine/leon3.h>
#include <dev/amba.h>

struct page_info *page_infos;

static void
leon3_detect_memory(void)
{
   struct amba_apb_device apb_dev;
    uint32_t r = amba_apbslv_device(VENDOR_ESA, ESA_MCTRL, &apb_dev, 0);
    if (!r)
	panic("unable to find memory controller on APB");

    uint32_t mcfg2_addr = apb_dev.start + 4;
    uint32_t mcfg2 = LEON_BYPASS_LOAD_PA(mcfg2_addr);

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

    cprintf("Physical address space layout:\n");
    cprintf(" 0x%08x-0x%08x (ROM)\n", ahb_dev.start[0], ahb_dev.stop[0]);
    cprintf(" 0x%08x-0x%08x (IO)\n", ahb_dev.start[1], ahb_dev.stop[1]);
    cprintf(" 0x%08x-0x%08x (RAM, %dM available)\n", 
	    ahb_dev.start[2], ahb_dev.stop[2], sdram_sz);
}

void
page_init(void)
{
    page_alloc_init();
    leon3_detect_memory();
}
