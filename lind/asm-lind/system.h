#ifndef __UM_SYSTEM_GENERIC_H
#define __UM_SYSTEM_GENERIC_H

#include <asm-x86_64/system.h>

#undef switch_to
#undef local_irq_save
#undef local_irq_restore
#undef local_irq_disable
#undef local_irq_enable
#undef local_save_flags
#undef local_irq_restore
#undef local_irq_enable
#undef local_irq_disable
#undef local_irq_save
#undef irqs_disable

extern void *switch_to(void *prev, void *next, void *last);

extern int lind_irq_enabled;
#define local_save_flags(flags) do { flags = lind_irq_enabled; } while (0)
#define local_irq_restore(flags) do { lind_irq_enabled = flags; } while (0)
#define local_irq_save(flags) do { flags = lind_irq_enabled; \
				   lind_irq_enabled = 0; } while (0)
#define local_irq_enable() do { lind_irq_enabled = 1; } while (0)
#define local_irq_disable() do { lind_irq_enabled = 0; } while (0)
#define irqs_disabled() (!lind_irq_enabled)

extern void *_switch_to(void *prev, void *next, void *last);
#define switch_to(prev, next, last) prev = _switch_to(prev, next, last)

#endif
