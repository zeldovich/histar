#ifndef JOS_MACHINE_TRAPCODES_H
#define JOS_MACHINE_TRAPCODES_H

#define T_RESET		0		/* reset */
#define T_UI		1		/* undefined instruction */
#define T_SWI		2		/* software interrupt */
#define T_PA		3		/* prefetch abort (instr fetch error) */
#define T_DA		4		/* data abort (data access error) */
#define T_UNUSED	5		/* impossible! */
#define T_IRQ		6		/* regular interrupt */
#define T_FIQ		7		/* ``fast'' interrupt */

#define NTRAPS 8

#endif
