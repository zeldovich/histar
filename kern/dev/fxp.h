#ifndef JOS_DEV_FXP_H
#define JOS_DEV_FXP_H

#include <machine/types.h>
#include <machine/thread.h>
#include <dev/pci.h>
#include <kern/segment.h>
#include <inc/netdev.h>

void	fxp_attach(struct pci_func *pcif);

void	fxp_macaddr(uint8_t *addrbuf);
int	fxp_add_buf(struct Segment *sg, uint64_t offset, netbuf_type type);

// The API for fxp_thread_wait is as follows:
//
//  * If the last call to fxp_thread_wait was with a different
//    waiter value (or this is the first time fxp_thread_wait is
//    called and waiter != 0), then all buffers for the card are
//    cleared and -E_AGAIN is returned.
//
//  * Otherwise, if the value gen is the same as the one returned
//    by the previous call to fxp_thread_wait, the thread is put
//    to sleep, and -E_RESTART is returned.
//
//  * Otherwise, a new generation number is returned (positive
//    64-bit signed int).

int64_t	fxp_thread_wait(struct Thread *t, uint64_t waiter, int64_t gen);

#endif
