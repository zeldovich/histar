#ifndef JOS_DEV_FXP_H
#define JOS_DEV_FXP_H

#include <machine/types.h>
#include <machine/thread.h>
#include <dev/pci.h>
#include <kern/segment.h>
#include <inc/netdev.h>

void	fxp_attach(struct pci_func *pcif);
void	fxp_intr();

void	fxp_macaddr(uint8_t *addrbuf);
int64_t	fxp_thread_wait(struct Thread *t, int64_t gen);
int	fxp_add_buf(struct Segment *sg, uint64_t offset, netbuf_type type);

#endif
