#ifndef JOS_MACHINE_BOOT_H
#define JOS_MACHINE_BOOT_H

#define	SYSXBOOT_EAX_MAGIC	0x910DFAA0

#ifndef __ASSEMBLER__

#define E820_RAM	1
#define E820_RESERVED	2
#define E820_ACPI	3
#define E820_NVS	4

#define E820MAX	128

#define SMAP	0x534d4150	/* ASCII "SMAP" */

struct e820entry {
	uint64_t addr;	/* start of memory segment */
	uint64_t size;	/* size of memory segment */
	uint32_t type;	/* type of memory segment */
} __attribute__((packed));

struct sysx_info {
    uint32_t extmem_kb;
    uint32_t cmdline;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint32_t vbe_mode;
    struct e820entry e820_map[E820MAX];
    uint8_t e820_nents;
} __attribute__ ((packed));

/* Physical address to load the bootstrap code for application processors
 * See boot/Makefrag and boot/bootother.S.
 */
#define APBOOTSTRAP 0x7000

#endif

#endif
