#include <kern/arch.h>
#include <kern/lib.h>
#include <kern/prof.h>
#include <kern/pstate.h>
#include <kern/kobj.h>
#include <kern/sched.h>
#include <kern/timer.h>
#include <kern/uinit.h>
#include <dev/goldfish_irq.h>
#include <dev/goldfish_timer.h>
#include <dev/goldfish_ttycons.h>
#include <dev/msm_cpufreq.h>
#include <dev/msm_gpio.h>
#include <dev/msm_irq.h>
#include <dev/msm_mddi.h>
#include <dev/msm_smd.h>
#include <dev/msm_timer.h>
#include <dev/msm_ttycons.h>
#include <dev/htcdream_keypad.h>
#include <dev/htcdream_reset.h>
#include <dev/htcdream_gpio.h>
#include <machine/arm.h>
#include <machine/asm.h>
#include <machine/atag.h>
#include <machine/pmap.h>
#include <machine/cpu.h>

#define BLINK

extern uint32_t	cpsr_get(void);
extern void	cpsr_set(uint32_t);

extern uint32_t exception_vector[];
extern uint32_t exception_vector_stack_addrs[];

void init(uint32_t, uint32_t, void *);

char boot_cmdline[256];
 
#define N_MEM_DESCS	10
static struct atag_mem mem_desc[N_MEM_DESCS];
static int nmem_desc;

#ifdef BLINK
static void
blinker()
{
	static int state = 0;

	// blink the external buttons
	htcdream_gpio_write(14, state);
	state = 1 - state;
}
#endif

static void
bss_init(void)
{
	extern char edata[], end[];
	memset(edata, 0, end - edata);
}

// Set up our exception vectors. We must enable high vectors (last 64k of KVA),
// allocate space, copy the branch and branch targets (defined in locore.S),
// and establish the mapping.
//
// Note that the mapping exists in the kernel's upper half and will be carried
// around in all user process page tables.
// 
// Also, set up exception stacks for each type.
static void
exception_init(void)
{
	void *exceptpg;
	struct Pagemap *pm2;
	int i;

	cp15_ctrl_set(cp15_ctrl_get() | CTRL_V);

	if (page_alloc(&exceptpg) != 0)
		panic("%s: failed to allocate exception vector page", __func__);
	memset(exceptpg, 0, PGSIZE);
	memcpy(exceptpg, exception_vector, 64);

	if (page_alloc((void **)&pm2) != 0)
		panic("%s: failed to allocate exception vector l2", __func__);
	memset(pm2, 0, PGSIZE);

	pm2->pm_ent[240] = kva2pa(exceptpg) | ARM_MMU_L2_TYPE_SMALL |
	    ARM_MMU_L2_SMALL_BUFFERABLE | ARM_MMU_L2_SMALL_CACHEABLE |
	    ARM_MMU_L2_SMALL_AP(ARM_MMU_AP_KRW);
	kpagemap.pm_ent[4095] = kva2pa(pm2) | ARM_MMU_L1_TYPE_COARSE;

	cp15_tlb_flush();

	// Create separate stacks for each exception vector.
	// Remember that ARM EABI requires 8 byte sp alignment.
	for (i = 0; i < 8; i++) {
		void *vp;
		if (page_alloc(&vp) != 0)
			panic("%s: failed to allocate exception vector stack",
			    __func__);
		exception_vector_stack_addrs[i] = (uint32_t)vp + PGSIZE;
cprintf("except. stack %d @0x%08x\n", i, (uint32_t)vp + PGSIZE);
	}	
}

// Perform necessary initialisation dependent on our temporary low memory
// mappings.
void __attribute__((__noreturn__))
init(uint32_t bid_hi, uint32_t bid_lo, void *kargs)
{
	struct atag *atp = kargs;
	uint32_t board_id = (bid_hi << 8) | bid_lo;
	uint32_t board_rev = 0xdeadbeef;
	unsigned int i;

	bss_init();

	/* early device init */
#if defined(JOS_ARM_GOLDFISH)
	goldfish_irq_init();
	goldfish_ttycons_init();
	goldfish_timer_init();
#elif defined(JOS_ARM_HTCDREAM)
	msm_irq_init(0xc0000000);
	msm_timer_init(0xc0100000, 7, MSM_TIMER_GP, 32768);
	msm_ttycons_init(0xa9c00000, 11);			// uart3
#endif

	while (atp != NULL && atp->words != 0 && atp->tag != ATAG_NONE) {
		switch (atp->tag) {
		case ATAG_CORE:
			break;

		case ATAG_MEM:
			cprintf("Memory segment: offset 0x%08x, len 0x%08x\n",
			    atp->u.mem.offset, atp->u.mem.bytes);
			if (nmem_desc != N_MEM_DESCS) {
				mem_desc[nmem_desc].offset =
				    atp->u.mem.offset;
				mem_desc[nmem_desc].bytes = atp->u.mem.bytes;
				nmem_desc++;
			}
			break;

		case ATAG_CMDLINE:
			cprintf("Boot cmdline: [%s]\n", atp->u.cmdline.cmdline);
			strncpy(boot_cmdline, atp->u.cmdline.cmdline,
			    sizeof(boot_cmdline));
			break;

		case ATAG_REVISION:
			board_rev = atp->u.revision.revision;
			break;

		default:
			cprintf("Unhandled ATAG: 0x%08x\n", atp->tag);
			break;
		}

		atp = (struct atag *)((uint32_t *)atp + atp->words);
	}

#if defined(JOS_ARM_HTCDREAM)
	/* seem to be missing an ATAG for memory? hard-code 96MB. */
	if (nmem_desc == 0) {
		mem_desc[0].offset = 0x10000000;
		mem_desc[0].bytes  = 96 * 1024 * 1024;
		nmem_desc = 1;
	}
#endif

	cprintf("Board ID: 0x%04x, Board Revision 0x%08x\n", board_id,
	    board_rev);
	cpu_identify();

	page_init(mem_desc, nmem_desc);

	exception_init();

	/* late device init */
#if defined(JOS_ARM_HTCDREAM)
	msm_cpufreq_init(0xc0100000);
	msm_gpio_init(0xa9200800, 0xa9300c00);
	msm_mddi_init(0xaa600000);
	msm_smd_init(0x01f00000, 1024*1024, 0xc0100000, 0, 5);
	htcdream_acoustic_init(0x01fe0000, 64 * 1024);
	htcdream_keypad_init(board_rev);
	htcdream_gpio_init(0x98000000);
	htcdream_backlight_init();
	htcdream_reset_init();

#ifdef BLINK
	static struct periodic_task blink_timer;
	blink_timer.pt_interval_msec = 1000;
	blink_timer.pt_fn = blinker;
	timer_add_periodic(&blink_timer);
#endif
#endif

	/* we've no more use for the low memory, so unmap it */
	for (i = 0; i < 2048; i++) {
		kpagemap.pm_ent[i] = 0;
	}
	cp15_tlb_flush();
	cprintf("Jettisoned low memory mappings.\n");

	/* make things less ridiculously slow... */ 
	cp15_ctrl_set(cp15_ctrl_get() | CTRL_C | CTRL_W | CTRL_Z | CTRL_I);

	kobject_init();
	sched_init();
	pstate_init();
	prof_init();
	user_init();

	cprintf("=== kernel ready, calling thread_run() ===\n");
	thread_run();
	panic("thread_run() returned?!?!?!\n");
}
