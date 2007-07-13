#ifndef JOS_INC_UTRAP_H
#define JOS_INC_UTRAP_H

#include <machine/utrap.h>

/* Assembly stubs */
void utrap_entry_asm(void);
void utrap_chain_dwarf2(void);
void utrap_ret(struct UTrapframe *utf)
    __attribute__((noreturn, JOS_UTRAP_GCCATTR));

/* C fault handler */
void utrap_entry(struct UTrapframe *utf)
    __attribute__((noreturn, JOS_UTRAP_GCCATTR));
int  utrap_init(void);
void utrap_set_handler(void (*fn)(struct UTrapframe *));

/* Mask / unmask traps, returns old value */
int  utrap_is_masked(void);
int  utrap_set_mask(int masked);
void utrap_set_cs(uint16_t nval);	/* x86 asm stub */

/* User-mode trap types */
#define UTRAP_USER_NOP		0x00000001
#define UTRAP_USER_SIGNAL	0x00000002
#define UTRAP_USER_RESERVED	0xff000000

#endif
