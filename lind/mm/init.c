#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/init.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/bootmem.h>
#include <linux/slab.h>
#include <linux/module.h>

#include <asm/setup.h>
#include <asm/segment.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/system.h>

#include <archcall.h>

unsigned long min_mapnr;
EXPORT_SYMBOL(min_mapnr);

void
mem_init(void)
{
    unsigned long tmp;

    /* Upper limit on whats available for mem_map */
    max_mapnr = (arch_env.phy_start + arch_env.phy_bytes) >> PAGE_SHIFT;
    num_physpages = (arch_env.phy_bytes) >> PAGE_SHIFT;
    /* Lower limit */
    min_mapnr = arch_env.phy_start >> PAGE_SHIFT;
    
    totalram_pages = free_all_bootmem();

    tmp = nr_free_pages() << PAGE_SHIFT;
    printk(KERN_INFO "Memory available: %lu/%lu RAM\n", 
	   tmp, arch_env.phy_bytes);
}

void
show_mem(void)
{
    unsigned long pfn;
    int total = 0, free = 0;
    int shared = 0, cached = 0;
    int reserved = 0;
    struct page *page;
    
    printk(KERN_INFO "Mem-info:\n");
    show_free_areas();
    pfn = max_mapnr;
    while(--pfn >= min_mapnr) {
	page = pfn_to_page(pfn);
	total++;
	if(PageReserved(page))
	    reserved++;
	else if(PageSwapCache(page))
	    cached++;
	else if (!page_count(page))
	    free++;
	else if(page_count(page))
	    shared += page_count(page) - 1;
    }
    printk(KERN_INFO "%d pages of RAM\n", total);
    printk(KERN_INFO "%d free pages\n", free);
    printk(KERN_INFO "%d reserved pages\n", reserved);
    printk(KERN_INFO "%d pages shared\n", shared);
    printk(KERN_INFO "%d pages swap cached\n", cached);
}
