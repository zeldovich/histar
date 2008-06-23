// Intel MultiProcessor Specification:
// http://www.intel.com/design/pentium/datashts/24201606.pdf

#include <machine/mp.h>
#include <machine/x86.h>
#include <kern/arch.h>
#include <kern/lib.h>

enum { mp_debug = 0 };

uint32_t ncpu;
struct cpu cpus[JOS_NCPU];
struct cpu *bcpu;

struct mp_fptr {
    uint8_t signature[4];	// "_MP_"
    uint32_t physaddr;		// phys addr of MP config table
    uint8_t length;		// 1
    uint8_t specrev;		// [14]
    uint8_t checksum;		// all bytes must add up to 0
    uint8_t type;		// MP system config type
    uint8_t imcrp;
    uint8_t reserved[3];
};

struct mp_conf {
    uint8_t signature[4];	// "PCMP"
    uint16_t length;		// total table length
    uint8_t version;		// [14]
    uint8_t checksum;		// all bytes must add up to 0
    uint8_t product[20];	// product id
    uint32_t oemtable;		// OEM table pointer
    uint16_t oemlength;		// OEM table length
    uint16_t entry;		// entry count
    uint32_t lapicaddr;		// address of local APIC
    uint16_t xlength;		// extended table length
    uint8_t xchecksum;		// extended table checksum
    uint8_t reserved;
};

struct mp_proc {
    uint8_t type;		// entry type (0)
    uint8_t apicid;		// local APIC id
    uint8_t version;		// local APIC verison
    uint8_t flags;		// CPU flags
    uint8_t signature[4];	// CPU signature
    uint32_t feature;		// feature flags from CPUID instruction
    uint8_t reserved[8];
};

struct mp_ioapic {
    uint8_t type;		// entry type (2)
    uint8_t apicno;		// I/O APIC id
    uint8_t version;		// I/O APIC version
    uint8_t flags;		// I/O APIC flags
    uint32_t addr;		// I/O APIC address
};

#define MP_PROC    0x00		// One per processor
#define MP_BUS     0x01		// One per bus
#define MP_IOAPIC  0x02		// One per I/O APIC
#define MP_IOINTR  0x03		// One per bus interrupt source
#define MP_LINTR   0x04		// One per system interrupt source

#define MP_FLAGS_BOOT    0x02	// This proc is the bootstrap processor.

static uint8_t
sum(uint8_t * a, uint32_t length)
{
    uint8_t s = 0;
    for (uint32_t i = 0; i < length; i++)
	s += a[i];
    return s;
}

static struct mp_fptr *
mp_search1(physaddr_t pa, int len)
{
    uint8_t *start = (uint8_t *) pa2kva(pa);
    for (uint8_t * p = start; p < (start + len); p += sizeof(struct mp_fptr)) {
	if ((memcmp(p, "_MP_", 4) == 0)
	    && (sum(p, sizeof(struct mp_fptr)) == 0))
	    return (struct mp_fptr *) p;
    }
    return 0;
}

static struct mp_fptr *
mp_search(void)
{
    struct mp_fptr *ret;
    uint8_t *bda;
    physaddr_t pa;

    bda = (uint8_t *) pa2kva(0x400);
    if ((pa = ((bda[0x0F] << 8) | bda[0x0E]) << 4)) {
	if ((ret = mp_search1(pa, 1024)))
	    return ret;
    } else {
	pa = ((bda[0x14] << 8) | bda[0x13]) * 1024;
	if ((ret = mp_search1(pa - 1024, 1024)))
	    return ret;
    }
    return mp_search1(0xF0000, 0x10000);
}

uint32_t
arch_cpu(void)
{
    return (KSTACKTOP(0) - read_rsp()) / (3 * PGSIZE);
}

void
mp_init(void)
{
    // default values
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
	(sum((uint8_t *) conf, conf->length) != 0) ||
	(conf->version != 1 && conf->version != 4)) 
    {
	cprintf("mp_init: bad or unsupported configuration\n");
	return;
    }

    ncpu = 0;
    struct mp_proc *proc;
    struct mp_ioapic *ioapic_conf;
    uint8_t *start = (uint8_t *) (conf + 1);
    for (uint8_t * p = start; p < ((uint8_t *) conf + conf->length);) {
	switch (*p) {
	case MP_PROC:
	    proc = (struct mp_proc *) p;
	    p += sizeof(*proc);
	    if (ncpu == JOS_NCPU)
		panic("mp_init: out of space");
	    cpus[ncpu].apicid = proc->apicid;
	    if (proc->flags & MP_FLAGS_BOOT)
		bcpu = &cpus[ncpu];
	    ncpu++;
	    continue;
	case MP_IOAPIC:
	    p += sizeof(*ioapic_conf);
	    continue;
	case MP_IOINTR:
	    p += 8;
	    continue;
	case MP_LINTR:
	    p += 8;
	    continue;
	case MP_BUS:
	    p += 8;
	    continue;
	default:
	    panic("unknown config type %x\n", *p);
	}
    }

    if (fptr->imcrp) {
	// force NMI and 8259 signals to APIC
	outb(0x22, 0x70);
	outb(0x23, 0x01);
    }


    if (mp_debug)
	for (uint32_t i = 0; i < ncpu; i++)
	    cprintf(" cpu: %4u apic id: %4u\n", i, cpus[i].apicid);
}
