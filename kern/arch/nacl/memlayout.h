#ifndef JOS_MACHINE_MEMLAYOUT_H
#define JOS_MACHINE_MEMLAYOUT_H

#include <machine/mmu.h>

/*
 * Virtual memory map
 *
 *                    +---------------------------------------------+
 *                    |                                             |
 *                    |   Kernel memory (all symbols point here)    |
 *                    |                                             |
 *   KBASE     -----> +---------------------------------------------+
 *                    |         user-kernel stack two pages         |
 *  UKSTACK    -----> +---------------------------------------------+
 *                    |          user-kernel scratch page           |
 *  UKSCRATCH  -----> +---------------------------------------------+
 *                    |        user-kernel syscall stub page        |
 *  UKSYSCAL   -----> +---------------------------------------------+
 *                    |       user-kernel segment switch page       |
 *  UKSWITCH   -----> +---------------------------------------------+
 *                    |                one page gap                 |
 *   ULIM --------->  +---------------------------------------------+
 *                    |                 user stack                  |
 *                    |                 user data                   |
 *                    |                 user text                   |
 *                    |                    ...                      |
 *   0 ------------>  +---------------------------------------------+
 */

#define KBASE		0x80008000

#define UKSTACK		0x80006000
#define UKSCRATCH2	0x80005800
#define UKSCRATCH	0x80005000
#define UKSYSCALL	0x80004000
#define UKINFO		0x80003000
#define UKSWITCH	0x80002000

#define ULIM		0x80000000
#define USTACKTOP	0x80000000
#define UMMAPBASE	0x50000000
#define UHEAP		0x60000000
#define UHEAPTOP	0x70000000
#define USTARTENVRO	0x70001000
#define UTLSBASE	0x70002000
#define UTLSTOP		0x70fff000
#define UFDBASE		0x71000000
#define USEGMAPENTS	0x72000000
#define ULDSO		0x73000000
#define UTLS_DEFSIZE	PGSIZE

#endif
