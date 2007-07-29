#ifndef JOS_MACHINE_TRAPCODES_H
#define JOS_MACHINE_TRAPCODES_H

/*
 * Processor-defined traps
 */
#define T_RESET		0x00	/* reset */
#define T_TEXTFAULT	0x01	/* instruction_access_exception */
#define T_ILLOP		0x02	/* illegal_instruction */
#define T_PRIVOP	0x03	/* privileged_instruction */
#define T_FP_DISABLED	0x04	/* fp_disabled */
#define	T_WIN_OF	0x05	/* window_overflow */
#define T_WIN_UF	0x06	/* window_underflow */
#define T_ALIGN		0x07	/* mem_address_not_aligned */
#define T_FP		0x08	/* fp_exception */
#define T_DATAFAULT	0x09	/* data_access_exception */
#define T_TAG_OF	0x0a	/* tag_overflow */
#define T_WATCHPOINT	0x0b	/* watchpoint_detected */

#define T_IRQOFFSET	0x10	/* IRQ 1..15 are traps 0x11 thru 0x1f */

#define T_REG_ERR	0x20	/* r_register_access_error */
#define T_TEXT_ERR	0x21	/* instruction_access_error */
#define T_CP_DISABLED	0x24	/* cp_disabled */
#define T_FLUSH_UNIMPL	0x25	/* unimplemented_FLUSH */
#define T_CP		0x28	/* cp_exception */
#define T_DATA_ERR	0x29	/* data_access_error */
#define T_DIVIDE	0x2a	/* division_by_zero */
#define T_DATA_WR_ERR	0x2b	/* data_store_error */
#define T_DATA_MMU_MISS	0x2c	/* data_access_MMU_miss */
#define T_TEXT_MMU_MISS	0x3c	/* instruction_access_MMU_miss */

/*
 * Software-defined traps
 */
#define T_SOFTWARE_MIN  0x80

#define T_FLUSHWIN      0x83

#define T_SYSCALL	0x8A
#define T_BREAKPOINT	0x8B
#define T_EMUERR	0x8C
#define SOFTWARE_TRAP(num) ((num) - 0x80)

#define NTRAPS		0x100
#endif
