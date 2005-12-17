#ifndef JOS_DEV_PNIC_H
#define JOS_DEV_PNIC_H

#include <machine/types.h>
#include <machine/thread.h>
#include <dev/pci.h>
#include <kern/segment.h>
#include <inc/netdev.h>

void	pnic_attach(struct pci_func *pcif);

void	pnic_macaddr(uint8_t *addrbuf);
int	pnic_add_buf(struct Segment *sg, uint64_t offset, netbuf_type type);
int64_t	pnic_thread_wait(struct Thread *t, uint64_t waiter, int64_t gen);

#endif
