/*
 * Intel MultiProcessor Specification:
 *  http://www.intel.com/design/pentium/datashts/24201606.pdf
 *
 * Plan 9 multiprocessor code:
 *  plan9/sys/src/9/pc/mp.c
 */

#include <machine/mp.h>
#include <machine/x86.h>
#include <machine/trapcodes.h>
#include <dev/ioapic.h>
#include <dev/apicreg.h>
#include <dev/apic.h>
#include <kern/arch.h>
#include <kern/lib.h>
#include <kern/intr.h>

enum { mp_debug     = 0 };
enum { irq_print    = 1 };
enum { enable_print = 1 };

enum { MAX_APICNO = 31 };

uint32_t ncpu;
struct cpu cpus[JOS_NCPU];
struct cpu *bcpu;

static int mpisabus = -1;
static int mpeisabus = -1;
static int ionoguess = 16;
static struct bus* mpbus;
static struct bus* mpbuslast;
static struct apic mpapic[MAX_APICNO + 1];
static struct aintr *mpintr[MAX_TRAP + 1];

static const char* buses[] = {
    "CBUSI ",
    "CBUSII",
    "EISA  ",
    "FUTURE",
    "INTERN",
    "ISA   ",
    "MBI   ",
    "MBII  ",
    "MCA   ",
    "MPI   ",
    "MPSA  ",
    "NUBUS ",
    "PCI   ",
    "PCMCIA",
    "TC    ",
    "VL    ",
    "VME   ",
    "XPRESS",
    0,
};

static void
mpprint(void)
{
    cprintf("mp table:\n");
    for (uint32_t i = 0; i < ncpu; i++)
	cprintf(" cpu: %4u apic id: %4u\n", i, cpus[i].apicid);

    struct bus* bus;
    for (bus = mpbus; bus; bus = bus->next) {
	cprintf("bus, type %s, busno %u:\n", buses[bus->type], bus->busno);
	struct aintr *aintr;
	for (aintr = bus->aintr; aintr; aintr = aintr->next)
	    cprintf(" irq: %4u intin %4u apicno %4u\n", 
		    aintr->intr.irq, aintr->intr.intin, aintr->intr.apicno);
    }
}

static int
mpintrinit(struct bus* bus, struct mp_intr* intr, int vno)
{
    int el, po, v;
    
    /*
     * Parse an IO or Local APIC interrupt table entry and
     * return an encoded vector suitable for the IOAPIC.
     */
    v = vno;

    po = intr->flags & PCMP_POMASK;
    el = intr->flags & PCMP_ELMASK;

    switch (intr->intr) {
	
    case PCMP_INT:
	v |= LAPIC_DLMODE_LOW;
	break;
	
    case PCMP_NMI:
	v |= LAPIC_DLMODE_NMI;
	po = PCMP_HIGH;
	el = PCMP_EDGE;
	break;
	
    case PCMP_SMI:
	v |= LAPIC_DLMODE_SMI;
	break;
	
    case PCMP_EXTINT:
	v |= LAPIC_DLMODE_EXTINT;
	/* Plan 9 says:
	 * The AMI Goliath doesn't boot successfully with it's LINTR0
	 * entry which decodes to low+level. The PPro manual says ExtINT
	 * should be level, whereas the Pentium is edge. Setting the
	 * Goliath to edge+high seems to cure the problem. Other PPro
	 * MP tables (e.g. ASUS P/I-P65UP5 have a entry which decodes
	 * to edge+high, so who knows.
	 * Perhaps it would be best just to not set an ExtINT entry at
	 * all, it shouldn't be needed for SMP mode.
	 */
	po = PCMP_HIGH;
	el = PCMP_EDGE;
	break;

    default:
	panic("bad interrupt type %u", intr->intr);
    }
    
    if (bus->type == BUS_EISA && !po && !el) {
	po = PCMP_HIGH;
	el = PCMP_EDGE;
    }
    if (!po)
	po = bus->po;
    if (po == PCMP_LOW)
	v |= LAPIC_INP_POL;  /* Active low */
    else if (po != PCMP_HIGH) {
	cprintf("mpintrinit: bad polarity 0x%x\n", po);
	return LAPIC_VT_MASKED;
    }
    
    if (!el)
	el = bus->el;
    if (el == PCMP_LEVEL)
	v |= LAPIC_VT_LEVTRIG;
    else if (el != PCMP_EDGE) {
	cprintf("mpintrinit: bad trigger 0x%x\n", el);
	return LAPIC_VT_MASKED;
    }
    
    return v;
}

static struct bus *
mpgetbus(uint32_t busno)
{
    struct bus* bus;
    for (bus = mpbus; bus; bus = bus->next)
	if (bus->busno == busno)
	    return bus;

    cprintf("mpgetbus: can't find bus %d\n", busno);
    return 0;
}

static struct aintr*
mpintrfind(uint32_t irq, tbdp_t tbdp)
{
    struct bus *bus;
    struct aintr *aintr;
    uint32_t type, bno, dno;

    /*
     * Find the bus.
     */
    type = BUSTYPE(tbdp);
    bno = BUSBNO(tbdp);
    dno = BUSDNO(tbdp);
    if (type == BUS_ISA)
	bno = mpisabus;
    for (bus = mpbus; bus; bus = bus->next) {
	if (bus->type != type)
	    continue;
	if (bus->busno == bno)
	    break;
    }

    if (!bus) {
	if (irq_print)
	    cprintf("mpintrfind: can't find bus type %d\n", type);
	return 0;
    }

    /*
     * For PCI devices the interrupt pin (INT[ABCD]) and device
     * number are encoded into the irq field of the interrupt table entry 
     * in the MP configuration.  We need to find the correct interrupt
     * table entry, intr, and unmask the redirection entry corresponding to
     * intr->intin.  To find intr we create the "irq" to match on.  See 
     * Intel MultiProcessor specification appendix D-3 for more info.
     */
    if (bus->type == BUS_PCI) {
	uint32_t pno = BUSPNO(tbdp);
	if (!pno)
	    panic("PCI intin is 0!?");
	irq = (dno << 2) | (pno - 1);
    }

    /*
     * Find a matching interrupt entry from the list of interrupts
     * attached to this bus.
     */
    for (aintr = bus->aintr; aintr; aintr = aintr->next) {
	if (aintr->intr.irq != irq)
	    continue;
	return aintr;
    }
    return 0;
}

void
mp_intrenable(trapno_t tno)
{
    /*
     * This function unmasks the interrupt on CPU 0.
     */

    struct aintr* aintr;
    struct apic* apic;
    uint32_t lo;

    assert(mpintr[tno]);
    aintr = mpintr[tno];
    apic = aintr->apic;

    if (enable_print)
	cprintf("tno %u enabled\n", tno);

    ioapic_rdtr(apic, aintr->intr.intin, 0, &lo);
    ioapic_rdtw(apic, aintr->intr.intin, 0, lo & ~LAPIC_VT_MASKED);
}

void
mp_intrdisable(uint32_t tno)
{
    /*
     * This function masks the interrupt on all CPUs.
     */

    struct aintr* aintr;
    struct apic* apic;
    uint32_t lo;

    assert(mpintr[tno]);
    aintr = mpintr[tno];
    apic = aintr->apic;

    if (enable_print)
	cprintf("tno %u disabled\n", tno);
    
    ioapic_rdtr(apic, aintr->intr.intin, 0, &lo);
    ioapic_rdtw(aintr->apic, aintr->intr.intin, 0, lo | LAPIC_VT_MASKED);    
}

trapno_t
mp_intrinit(irqno_t irq, tbdp_t tbdp, trapno_t trapno)
{
    struct aintr* aintr = 0;
    struct bus* bus;
    struct apic* apic;
    uint32_t lo;

    assert(mpintr[trapno] == 0);
    assert(irq <= MAX_IRQ_PIC);

    /*
     * If the bus is known, try it.
     * BUSUNKNOWN is given by [E]ISA devices.
     */
    if (tbdp != BUSUNKNOWN) 
	aintr = mpintrfind(irq, tbdp);
    if (!aintr && mpeisabus != -1)
	aintr = mpintrfind(irq, MKBUS(BUS_EISA, 0, 0, 0));
    if (!aintr && mpisabus != -1)
	aintr = mpintrfind(irq, MKBUS(BUS_ISA, 0, 0, 0));

    if (!aintr)
	panic("couldn't find intr for irq %u tbdp %x trapno %u",
	      irq, tbdp, trapno);

    apic = aintr->apic;
    bus = mpgetbus(aintr->intr.busno);
    assert(bus);

    ioapic_rdtr(apic, aintr->intr.intin, 0, &lo);
    if (lo & 0xFF) {
	trapno = lo & 0xFF;
	uint32_t n = mpintrinit(bus, &aintr->intr, trapno);
	lo &= ~(LAPIC_DLMODE_RR | LAPIC_VT_DELIVS);
	if (n != lo || !(n & LAPIC_VT_LEVTRIG))
	    panic("multiple botch irq %d, tbdp %x, lo %x, n %x",
		  irq, tbdp, lo, n);
	
	cprintf("mp_intrinit: multiple to tno %u\n", trapno);
	return trapno;
    }

    lo = mpintrinit(bus, &aintr->intr, trapno);
    if (lo & LAPIC_VT_MASKED)
	panic("confused by irq %u tbdp %x tno %u", irq, tbdp, trapno);

    if ((apic->flags & PCMP_EN) && apic->type == PCMP_IOAPIC)
	ioapic_rdtw(apic, aintr->intr.intin, 0, lo | LAPIC_VT_MASKED);
    else
	panic("wierd APIC flags %x type %u", apic->flags, apic->type);

    aintr->vno = trapno;
    mpintr[trapno] = aintr;
    return trapno;
}

static void
mkbus(struct mp_bus* p)
{
    static struct bus bus_[16];
    static uint32_t nbus;
    if (nbus == 16)
	panic("not enough mp_intrs");
    
    struct bus* bus;
    int i;
    
    for (i = 0; buses[i]; i++) {
	if (strncmp(buses[i], p->busstr, sizeof(p->busstr)) == 0)
	    break;
    }
    
    bus = &bus_[nbus++];
    if (mpbus)
	mpbuslast->next = bus;
    else
	mpbus = bus;
    mpbuslast = bus;
    
    bus->type = i;
    bus->busno = p->busno;
    
    if (bus->type == BUS_EISA) {
	bus->po = PCMP_LOW;
	bus->el = PCMP_LEVEL;
	if(mpeisabus != -1)
	    cprintf("mkbus: more than one EISA bus\n");
	mpeisabus = bus->busno;
    }
    else if (bus->type == BUS_PCI) {
	bus->po = PCMP_LOW;
	bus->el = PCMP_LEVEL;
    }
    else if (bus->type == BUS_ISA) {
	bus->po = PCMP_HIGH;
	bus->el = PCMP_EDGE;
	if (mpisabus != -1)
	    cprintf("mkbus: more than one ISA bus\n");
	mpisabus = bus->busno;
    }
    else {
	bus->po = PCMP_HIGH;
	bus->el = PCMP_EDGE;
    }
}

static void
mkiointr(struct mp_intr *p)
{
    static struct aintr intr_[128];
    static uint32_t nintr;
    if (nintr == 256)
	panic("not enough struct intrs");

    struct bus *bus;
    if ((bus = mpgetbus(p->busno)) == 0)
	return;
    
    struct aintr *aintr = &intr_[nintr++];
    aintr->apic = &mpapic[p->apicno];
    aintr->next = bus->aintr;
    memcpy(&aintr->intr, p, sizeof(aintr->intr));
    bus->aintr = aintr;

    if (ionoguess != -1)
	ionoguess = p->apicno;
}

static void
mkioapic(struct mp_ioapic* p)
{
    struct apic *apic;

    if (!(p->flags & PCMP_EN) || p->apicno > MAX_APICNO)
	return;
    
    apic = &mpapic[p->apicno];
    apic->type = PCMP_IOAPIC;
    apic->apicno = p->apicno;
    apic->addr = pa2kva(p->addr);
    apic->paddr = p->addr;
    apic->flags = p->flags;
    ionoguess = -1;

    ioapic_init(apic);
}

void
mp_init(void)
{
    /* default values */
    ncpu = 1;
    bcpu = &cpus[0];

    struct mp_fptr *fptr = mp_search();
    if (!fptr) {
	cprintf("mp_init: no MP floating pointer found\n");
	return;
    }
    if (fptr->physaddr == 0 || fptr->type != 0) {
	cprintf("mp_init: legacy MP configurations not supported\n");
	return;
    }

    struct mp_conf *conf = pa2kva(fptr->physaddr);
    if ((memcmp(conf->signature, "PCMP", 4) != 0) ||
	(conf->version != 1 && conf->version != 4)) 
    {
	cprintf("mp_init: bad or unsupported configuration\n");
	return;
    }

    ncpu = 0;
    struct mp_proc *proc;
    uint8_t *start = (uint8_t *) (conf + 1);
    for (uint8_t * p = start; p < ((uint8_t *) conf + conf->length);) {
	switch (*p) {
	case PCMP_PROCESSOR:
	    proc = (struct mp_proc *) p;
	    p += sizeof(*proc);
	    if (ncpu == JOS_NCPU)
		panic("mp_init: out of space");
	    cpus[ncpu].apicid = proc->apicid;
	    if (proc->flags & PCMP_BP)
		bcpu = &cpus[ncpu];
	    ncpu++;
	    continue;
	case PCMP_IOAPIC:
	    mkioapic((struct mp_ioapic *) p);
	    p += sizeof(struct mp_ioapic);
	    continue;
	case PCMP_IOINTR:
	    mkiointr((struct mp_intr *) p);
	    p += sizeof(struct mp_intr);
	    continue;
	case PCMP_LINTR:
	    p += sizeof(struct mp_intr);;
	    continue;
	case PCMP_BUS:
	    mkbus((struct mp_bus *) p);
	    p += sizeof(struct mp_bus);
	    continue;
	default:
	    panic("unknown config type %x\n", *p);
	}
    }

    /* 
     * Some BIOSs don't populate the configuration table with 
     * an IOAPIC.  So we hope there is only one IOAPIC, guess 
     * its apicno, fake an entry, and hope it works.
     */
    if (ionoguess != -1) {
	struct mp_ioapic iop;
	iop.type = PCMP_IOAPIC;
	iop.flags = PCMP_EN;
	iop.addr = 0xFEC00000;	/* default physical address */
	iop.version = 1;	/* original MP version */
	iop.apicno = ionoguess;
	mkioapic(&iop);
    }

    if (fptr->imcrp) {
	/* force NMI and 8259 signals to APIC */
	outb(0x22, 0x70);
	outb(0x23, 0x01);
    }

    if (mp_debug)
	mpprint();
}
