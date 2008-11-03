#ifndef JOS_MACHINE_LNXOPTS_H
#define JOS_MACHINE_LNXOPTS_H

/*
 * Clean up user-kernel stack frame pairs when control
 * flow goes back to a user thread again.
 */
enum { lnx64_stack_gc = 0 };

#endif
