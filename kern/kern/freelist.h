#ifndef FREELIST_H_
#define FREELIST_H_

#include <lib/btree/btree.h>
#include <lib/btree/btree_impl.h>

// freelist resource manager
// prevents the freelist from modifying the btrees, while they
// are being modified by a call to freelist_alloc or freelist_free
struct frm
{
	// XXX: make frm like btree_default
	struct btree_simple simple ;

#define FRM_BUF_SIZE 10
	uint64_t to_use[FRM_BUF_SIZE] ;
	uint64_t to_free[FRM_BUF_SIZE] ;
	
	int n_use ;
	int n_free ;

	uint8_t service ;
	uint8_t servicing ;
} ;

struct freelist
{
	struct btree chunks ;
	struct btree offsets ;
	
	struct frm offset_frm ;
	struct frm chunk_frm ;
	
	
	uint64_t free ;
} ;

int 	freelist_init(struct freelist *l, uint64_t offset, uint64_t npages) ;
int 	freelist_free(struct freelist *l, uint64_t base, uint64_t npages) ;
void	freelist_setup(uint8_t *b) ;
int64_t freelist_alloc(struct freelist *l, uint64_t npages) ;
void 	freelist_serialize(struct freelist *f) ;

// debug
void freelist_pretty_print(struct freelist *l) ;
void freelist_sanity_check(struct freelist *l) ;



#endif /*FREELIST_H_*/
