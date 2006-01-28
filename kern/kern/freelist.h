#ifndef FREELIST_H_
#define FREELIST_H_

#include <lib/btree/btree.h>
#include <lib/btree/btree_cache.h>

struct freelist
{
	struct btree chunks ;
	struct btree offsets ;
	
	uint64_t free ;
} ;

int 	freelist_init(struct freelist *l, uint64_t offset, uint64_t npages) ;
int 	freelist_free(struct freelist *l, uint64_t base, uint64_t npages) ;
int64_t freelist_alloc(struct freelist *l, uint64_t npages) ;

// debug
void freelist_pretty_print(struct freelist *l) ;
void freelist_sanity_check(struct freelist *l) ;



#endif /*FREELIST_H_*/
