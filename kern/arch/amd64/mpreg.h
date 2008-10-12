#ifndef JOS_MACHINE_MPREG_H
#define JOS_MACHINE_MPREG_H

struct mp_fptr {		/* floating pointer */
    uint8_t    signature[4];	/* "_MP_" */
    uint32_t   physaddr;	/* phys addr of MP config table */
    uint8_t    length;		/* 1 */
    uint8_t    specrev;		/* [14] */
    uint8_t    checksum;	/* all bytes must add up to 0 */
    uint8_t    type;		/* MP system config type */
    uint8_t    imcrp;
    uint8_t    reserved[3];
};

struct mp_conf {		/* configuration table header */
    uint8_t    signature[4];	/* "PCMP" */
    uint16_t   length;		/* total table length */
    uint8_t    version;		/* [14] */
    uint8_t    checksum;	/* all bytes must add up to 0 */
    uint8_t    product[20];	/* product id */
    uint32_t   oemtable;	/* OEM table pointer */
    uint16_t   oemlength;	/* OEM table length */
    uint16_t   entry;		/* entry count */
    uint32_t   lapicaddr;	/* address of local APIC */
    uint16_t   xlength;		/* extended table length */
    uint8_t    xchecksum;	/* extended table checksum */
    uint8_t    reserved;
};

struct mp_proc {		/* processor table entry */
    uint8_t    type;		/* entry type (0) */
    uint8_t    apicid;		/* local APIC id */
    uint8_t    version;		/* local APIC verison */
    uint8_t    flags;		/* CPU flags */
    uint8_t    signature[4];	/* CPU signature */
    uint32_t   feature;		/* feature flags from CPUID instruction */
    uint8_t    reserved[8];
};

struct mp_bus {			/* bus table entry */
    uint8_t   type;		/* entry type(1) */
    uint8_t   busno;		/* bus id */
    char      busstr[6];	/* string which identifies the type of this bus */
};

struct mp_ioapic {		/* IOAPIC table entry */
    uint8_t    type;		/* entry type (2) */
    uint8_t    apicno;		/* IOAPIC id */
    uint8_t    version;		/* IOAPIC version */
    uint8_t    flags;		/* IOAPIC flags */
    uint32_t   addr;		/* IOAPIC address */
};

struct mp_intr {		/* interrupt table entry */
    uint8_t    type;		/* entry type (3,4) */
    uint8_t    intr;		/* interrupt type */
    uint16_t   flags;		/* interrupt flag */
    uint8_t    busno;		/* source bus id */
    uint8_t    irq;		/* source bus irq */
    uint8_t    apicno;		/* destination APIC id */
    uint8_t    intin;		/* destination APIC [L]INTIN# */
};

enum {					/* table entry types */
    PCMP_PROCESSOR	= 0x00,		/* one entry per processor */
    PCMP_BUS		= 0x01,		/* one entry per bus */
    PCMP_IOAPIC		= 0x02,		/* one entry per I/O APIC */
    PCMP_IOINTR		= 0x03,		/* one entry per bus interrupt source */
    PCMP_LINTR		= 0x04,		/* one entry per system interrupt source */
    
    PCMP_SASM		= 0x80,
    PCMP_HIERARCHY	= 0x81,
    PCMP_CBASM		= 0x82,
    
    /* PCMP_PROCESSOR and PCMP_IOAPIC flags */
    PCMP_EN		= 0x01,		/* enabled */
    PCMP_BP		= 0x02,		/* bootstrap processor */
    
    /* PCMPiointr and PCMPlintr flags */
    PCMP_POMASK		= 0x03,		/* polarity conforms to specifications of bus */
    PCMP_HIGH		= 0x01,		/* active high */
    PCMP_LOW		= 0x03,		/* active low */
    PCMP_ELMASK		= 0x0C,		/* trigger mode of APIC input signals */
    PCMP_EDGE		= 0x04,		/* edge-triggered */
    PCMP_LEVEL		= 0x0C,		/* level-triggered */
    
    /* PCMP_IOINTR and PCMP_LINTR interrupt type */
    PCMP_INT	       = 0x00,		/* vectored interrupt from APIC Rdt */
    PCMP_NMI	       = 0x01,		/* non-maskable interrupt */
    PCMP_SMI	       = 0x02,		/* system management interrupt */
    PCMP_EXTINT	       = 0x03,		/* vectored interrupt from external PIC */
    
    /* PCMP_SASM addrtype */
    PCMP_IOADDR	      = 0x00,		/* I/O address */
    PCMP_MADDR	      = 0x01,		/* memory address */
    PCMP_PADDR	      = 0x02,		/* prefetch address */
    
    /* PCMP_HIERACHY info */
    PCMP_SD		= 0x01,		/* subtractive decode bus */
    
    /* PCMP_CBASM modifier */
    PCMP_PR		= 0x01,		/* predefined range list */
};

#endif
