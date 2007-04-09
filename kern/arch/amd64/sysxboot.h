#ifndef JOS_MACHINE_SYSXBOOT_H
#define JOS_MACHINE_SYSXBOOT_H

#define SYSXBOOT_EAX_MAGIC		0x910DFAA0

#ifndef __ASSEMBLER__

struct sysx_info {
    uint32_t extmem_kb;
    uint32_t cmdline;
};

#endif
#endif
