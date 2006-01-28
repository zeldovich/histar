#ifndef BT_CACHE_H_
#define BT_CACHE_H_

#include <lib/btree/btree.h>
#include <lib/btree/cache.h>
#include <inc/types.h>

// use to declare a btree cache
#define STRUCT_BTREE_CACHE(name, num_ent, order, key_size)	\
	STRUCT_CACHE(name##_cache, num_ent, BTREE_NODE_SIZE(order, key_size)) ; \
	struct btree_cache name = { (order), &(name##_cache) } ;

struct btree_cache
{
	uint8_t	order ;
	struct cache *cache ;	
} ;

int	btree_cache_node(struct btree *tree, offset_t offset, struct btree_node **store, void *manager) ;
int btree_cache_rem(void *arg, offset_t offset) ;
int btree_cache_alloc(struct btree *tree, offset_t offset, struct btree_node **store, void *manager) ;
int btree_cache_init(void *arg) ;
int btree_cache_pin_is(void *arg, offset_t offset, uint8_t pin) ;

#endif /*BT_CACHE_H_*/
