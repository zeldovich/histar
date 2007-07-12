#include <linux/highmem.h>
#include <linux/module.h>
#include <linux/bootmem.h>
#include <linux/initrd.h>
#include <linux/swap.h>
#include <linux/seq_file.h>
#include <asm/proto.h>

#include <archcall.h>
#include <longjmp.h>
#include <kern/process.h>

unsigned long *empty_zero_page;
EXPORT_SYMBOL(empty_zero_page);

/*
 * Machine setup..
 */
struct cpuinfo_lind boot_cpu_data = { 
	.loops_per_jiffy	= 0,
	.ipi_pipe		= { -1, -1 }
};
EXPORT_SYMBOL(boot_cpu_data);

arch_env_t arch_env;
EXPORT_SYMBOL(arch_env);

static int 
panic_exit(struct notifier_block *self, unsigned long unused1,
	   void *unused2)
{
    arch_halt(1);
    return(0);
}

static struct notifier_block panic_exit_notifier = {
    .notifier_call 		= panic_exit,
    .next 			= NULL,
    .priority 		        = 0
};


static int show_cpuinfo(struct seq_file *m, void *v)
{
    seq_printf(m, "model name\t: HiStar\n");
    return 0;
}

static void *c_start(struct seq_file *m, loff_t *pos)
{
    return *pos < NR_CPUS ? cpu_data + *pos : NULL;
}

static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
    ++*pos;
    return c_start(m, pos);
}

static void c_stop(struct seq_file *m, void *v)
{
}

const struct seq_operations cpuinfo_op = {
	.start	= c_start,
	.next	= c_next,
	.stop	= c_stop,
	.show	= show_cpuinfo,
};

/* 
 * Called very early by the Linux kernel start code
 */
void __init 
setup_arch(char **cmdline_p)
{
    unsigned long zones_size[MAX_NR_ZONES];
    memset(zones_size, 0, sizeof(zones_size));

    atomic_notifier_chain_register(&panic_notifier_list,
				   &panic_exit_notifier);
    
    /*
     * Initialize the bad page table and bad page to point
     * to a couple of allocated pages.
     */
    zones_size[ZONE_DMA] = 0;
    zones_size[ZONE_NORMAL] = arch_env.phy_bytes >> PAGE_SHIFT;
    free_area_init(zones_size);

    strlcpy(saved_command_line, arch_env.command_line, COMMAND_LINE_SIZE);
    *cmdline_p = arch_env.command_line;

    lind_signal_init();
}

static 
int start_kernel_proc(void *unused)
{
    block_signals();
    start_kernel();
    return 0;
}

void
start_lind(void)
{
    init_task.thread.request.u.thread.proc = start_kernel_proc;
    init_task.thread.request.u.thread.arg = NULL;

    start_idle_thread(task_stack_page(&init_task),
		      &init_task.thread.switch_buf);
}

static void
bootmem_init(void)
{
    unsigned long bootmap_size;
    
    unsigned long pa_start = arch_env.phy_start;
    unsigned long pa_end = arch_env.phy_start + arch_env.phy_bytes;
    unsigned long bootmap_start = pa_start;
    
    /*
     * Give all the memory to the bootmap allocator, tell it to put the
     * boot mem_map at the start of memory.
     */
    bootmap_size = init_bootmem_node(NODE_DATA(0), 
				     bootmap_start >> PAGE_SHIFT, 
				     pa_start >> PAGE_SHIFT,
				     pa_end >> PAGE_SHIFT);
    /*
     * Free the usable memory, we have to make sure we do not free
     * the bootmem bitmap so we then reserve it after freeing it :-)
     */
    free_bootmem(bootmap_start, pa_end - bootmap_start);
    reserve_bootmem(bootmap_start, bootmap_size);
    
    /*
     * Setup zero'ed page
     */
    empty_zero_page = alloc_bootmem_pages(PAGE_SIZE);
    memset(empty_zero_page, 0, PAGE_SIZE);
}

int 
lind_initrd_load(const char *pn)
{
    unsigned long size;
    void *mem;
    int r;
    r = arch_file_size(pn, &size);
    if (r < 0) {
	arch_printf("lind_initrd_load: unable to stat %s\n", pn);
	return -1;
    }
    if (!size) {
	arch_printf("lind_initrd_load: zero byte initrd image %s\n", pn);
	return -1;
    }

    mem = __alloc_bootmem(ALIGN(size, PAGE_SIZE), PAGE_SIZE, 0);

    if (mem == NULL) {
	arch_printf("lind_initrd_load: unable to alloc %ld bytes\n", size);
	return -1;
    }

    r = arch_file_read(pn, mem, size);
    if (r < 0) {
	/* clean up bootmem */
	arch_printf("lind_initrd_load: unable to read %s\n", pn);
	return -1;
    }
    
    initrd_start = (unsigned long) mem;
    initrd_end = initrd_start + size;
    return 0;
}

void 
free_initrd_mem(unsigned long start, unsigned long end)
{
    if (start < end)
	printk ("Freeing initrd memory: %ld bytes (%ld pages)\n", 
		(end - start), ALIGN((end - start), PAGE_SIZE) / PAGE_SIZE);
    for (; start < end; start += PAGE_SIZE) {
	ClearPageReserved(virt_to_page(start));
	init_page_count(virt_to_page(start));
	free_page(start);
	totalram_pages++;
    }
}

void
command_line_init(int ac, char **av)
{
    char *cl = arch_env.command_line;
    unsigned int i, n;
    int flag = -1;

    n = strlen("initrd=");

    for (i = 1; i < ac; i++) {
	char *parm = av[i];

	if (strlen(cl) + strlen(parm) + 1 > COMMAND_LINE_SIZE) {
	    arch_printf("Too many command line arguments\n");
	    return;
	}
	
	if(strlen(cl) > 0)
	    strcat(cl, " ");
	strcat(cl, parm);

	if (!strncmp(parm, "initrd=", n)) 
	    flag = lind_initrd_load(parm + n);
    }

    if (flag < 0) {
	const char *def_initrd = "/bin/initrd";
	arch_printf("no initrd provided, trying %s\n", def_initrd);
	lind_initrd_load(def_initrd);
    }
}

void
linux_main(int ac, char **av)
{
    arch_init();
    bootmem_init();
    command_line_init(ac, av);

    start_lind();
}
