#ifndef LINUX_ARCH_INCLUDE_BARRIER_H
#define LINUX_ARCH_INCLUDE_BARRIER_H

#define mb() 	asm volatile("mfence":::"memory")

#endif
