#ifndef JOS_MACHINE_BOOT_H
#define JOS_MACHINE_BOOT_H

#define DIRECT_BOOT_EAX_MAGIC	0x6A6F7362
#define	SYSXBOOT_EAX_MAGIC	0x910DFAA0

#ifndef __ASSEMBLER__

struct sysx_info {
    uint32_t extmem_kb;
    uint32_t cmdline;
};

#endif

#endif
