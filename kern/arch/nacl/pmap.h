#ifndef JOS_MACHINE_PMAP_H
#define JOS_MACHINE_PMAP_H

#ifndef __ASSEMBLER__
#include <machine/types.h>
#endif

#define GD_KT       (0x08 | 0x00)       /* Kernel text */
#define GD_KD       (0x10 | 0x00)       /* Kernel data */
#define GD_TSS      (0x18 | 0x00)       /* Task segment selector */
#define GD_UD       (0x28 | 0x03)       /* User data */
#define GD_TD       (0x30 | 0x03)       /* Thread-local data */
#define GD_UT_NMASK (0x38 | 0x03)       /* User text, traps not masked */
#define GD_UT_MASK  (0x40 | 0x03)       /* User text, traps masked */

#ifndef __ASSEMBLER__
typedef uint64_t ptent_t;

struct Pagemap {
};
#endif

#endif
