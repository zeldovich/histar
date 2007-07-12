#ifndef LINUX_ARCH_INCLUDE_ARCHENV_H
#define LINUX_ARCH_INCLUDE_ARCHENV_H

#include <asm-lind/setup.h>

typedef struct {
    unsigned long phy_start; /* start of physical mem */
    unsigned long phy_bytes; /* number of bytes */

    char command_line[COMMAND_LINE_SIZE];
    
} arch_env_t;

extern arch_env_t arch_env;

#define MEMORY_SIZE  (arch_env.phy_bytes)
#define MEMORY_START (arch_env.phy_start)
#define MEMORY_END   (MEMORY_SIZE + MEMORY_START)

#endif
