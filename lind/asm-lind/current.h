#ifndef _LIND_CURRENT_H
#define _LIND_CURRENT_H

#ifndef __ASSEMBLY__

struct task_struct;
#include <asm/pda.h>
#define current (current_thread_info()->task)

#endif

#endif /* !(_X86_64_CURRENT_H) */
