#include <kern/arch.h>
#include <kern/lib.h>
#include <kern/prof.h>
#include <kern/pstate.h>
#include <kern/kobj.h>
#include <kern/sched.h>
#include <kern/uinit.h>
#include <dev/goldfish_irq.h>
#include <dev/goldfish_timer.h>
#include <dev/goldfish_ttycons.h>
#include <dev/msm_ttycons.h>
#include <machine/arm.h>
#include <machine/asm.h>
#include <machine/atag.h>
#include <machine/pmap.h>

extern uint32_t	cpsr_get(void);
extern void	cpsr_set(uint32_t);

extern uint32_t exception_vector[];
extern uint32_t exception_vector_stack_addrs[];

void init(int, int, void *);

char boot_cmdline[256];
 
#define N_MEM_DESCS	10
static struct atag_mem mem_desc[N_MEM_DESCS];
static int nmem_desc;

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
// XXX- icache flush?
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
	    ARM_MMU_L2_SMALL_AP(ARM_MMU_AP_KRW);
	kpagemap.pm_ent[4095] = kva2pa(pm2) | ARM_MMU_L1_TYPE_COARSE;

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
init(int bid_hi, int bid_lo, void *kargs)
{
	struct atag *atp = kargs;
	int board_id = (bid_hi << 8) | bid_lo;
	unsigned int i;

	bss_init();

#if defined(JOS_ARM_GOLDFISH)
	goldfish_irq_init();
	goldfish_ttycons_init();
	goldfish_timer_init();
#elif defined(JOS_ARM_HTCDREAM)
	msm_ttycons_init(0xa9c00000, 11, 14);
	//msm_timer_init(...);
#else
#error unknown arm target
#endif

	cprintf("Board ID: 0x%04x\n", board_id);

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

	page_init(mem_desc, nmem_desc);

	exception_init();

	/* we've no more use for the low memory, so unmap it */
	for (i = 0; i < 2048; i++) {
		kpagemap.pm_ent[i] = 0;
	}
	cp15_tlb_flush();
	cprintf("Jettisoned low memory mappings.\n");

	kobject_init();
	sched_init();
	pstate_init();
	prof_init();
	user_init();

	cprintf("=== kernel ready, calling thread_run() ===\n");
	thread_run();
	panic("thread_run() returned?!?!?!\n");
}
