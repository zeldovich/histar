#ifndef BTREE_IMPL_H_
#define BTREE_IMPL_H_

#include <lib/btree/btree.h>
#include <lib/btree/cache.h>
#include <inc/types.h>

// max order for key size of 1
#define BTREE_MAX_ORDER1 252
// max order for key size of 2
#define BTREE_MAX_ORDER2 168	

// use to declare a cache for a btree
#define STRUCT_BTREE_CACHE(name, num_ent, order, key_size)	\
	STRUCT_CACHE(name, num_ent, BTREE_NODE_SIZE(order, key_size)) ;

// provides much of the functionality needed to manage btree nodes...
// doesn't provide functionality for allocating nodes persistently.
struct btree_simple
{
	uint8_t	order ;
	struct cache *cache ;	
} ;

// default kernel level btree manager...freelist is used for allocating
// nodes persistently.
struct btree_default
{
	struct btree_simple simple ;
	struct freelist *fl ;
	
	struct btree tree ;
} ;

// btree is always stored in memory and isn't backed by disk
struct btree_volatile
{
	offset_t off_count ;
	struct node_list nodes ;
	
	struct btree tree ;
} ; 

int btree_simple_init(struct btree_simple *sim, uint8_t order, 
					  struct cache *cache) ;
int	btree_simple_node(struct btree *tree, offset_t offset, 
					  struct btree_node **store, void *man) ;
int btree_simple_alloc(struct btree *tree, offset_t offset, 
					   struct btree_node **store, void *man) ;
int btree_simple_rem(void *man, offset_t offset) ;
int btree_simple_unpin_all(void *man) ;
int btree_simple_unpin_node(void *man, offset_t offset) ;
int btree_simple_write(struct btree_node *node, void *man) ;

int btree_default_init(struct btree_default *def, uint8_t order, 
				   uint8_t key_size, uint8_t value_size,
				   struct freelist *fl,struct cache *cache) ;
int btree_default_setup(struct btree_default *def, uint8_t order,
						struct freelist *fl, struct cache *cache) ;

int btree_volatile_init(struct btree_volatile *vol, uint8_t order, 
		  			   uint8_t key_size, uint8_t value_size) ;


#endif /*BTREE_IMPL_H_*/
