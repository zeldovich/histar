#ifndef BT_CACHE_H_
#define BT_CACHE_H_

#include <lib/btree/btree.h>
#include <lib/btree/cache.h>
#include <inc/types.h>

// use to declare a btree manager
#define STRUCT_BTREE_MAN(name, num_ent, order, key_size)	\
	STRUCT_CACHE(name##_cache, num_ent, BTREE_NODE_SIZE(order, key_size)) ; \
	struct btree_man name = { (order), &(name##_cache) } ;

// default kernel level btree manager
struct btree_man
{
	uint8_t	order ;
	struct cache *cache ;	
} ;

int	btree_man_node(struct btree *tree, 
					 offset_t offset, 
					 struct btree_node **store, 
					 void *man) ;

int btree_man_alloc(struct btree *tree, 
					  offset_t offset, 
					  struct btree_node **store, 
					  void *man) ;

int btree_man_rem(void *man, offset_t offset) ;
int btree_man_init(void *man) ;
int btree_man_pin_is(void *man, offset_t offset, uint8_t pin) ;
int btree_man_write(struct btree_node *node, void *man) ;

#endif /*BT_CACHE_H_*/
