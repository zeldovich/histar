#include <lib/btree/btree_cache.h>
#include <inc/error.h>
#include <lib/btree/cache.h>
#include <inc/string.h>
#include <inc/assert.h>

#define CENT_NODE(ENT) ((struct btree_node *)ENT)
#define CENT_CHILDREN(ENT) ((offset_t *)((uint8_t *)ENT + sizeof(struct btree_node)))
#define CENT_KEYS(ENT, ORDER) ((uint64_t *)((uint8_t *)ENT + sizeof(struct btree_node) + \
							  sizeof(offset_t) * ORDER))

int 
btree_cache_node(struct btree *tree, offset_t offset, struct btree_node **store, void *manager)
{
	struct btree_cache * cache = (struct btree_cache *) manager ;
	int r ;	


	if ((r = cache_ent(cache->cache, offset, (uint8_t**) store)) < 0) {
		*store = 0 ;
		return -E_NOT_FOUND ;
	}
	return 0 ;
}

int 
btree_cache_alloc(struct btree *tree, offset_t offset, struct btree_node **store, void *manager)
{
	struct btree_cache *cache = (struct btree_cache *) manager ;
	struct btree_node 	*node ;
	
	int 	r ;	
	uint8_t *buf ;

	if ((r = cache_alloc(cache->cache, offset, &buf)) < 0) {
		*store = 0 ;
		return -E_NO_SPACE ;
	}
	
	node = CENT_NODE(buf) ;
	memset(node, 0, sizeof(struct btree_node)) ;

	// setup pointers in node
	node->children = CENT_CHILDREN(buf) ;
	node->keys = CENT_KEYS(buf, cache->order) ;
	node->block.offset = offset ;
	node->tree = tree ;

	*store = node ;
	return 0 ;
}

int 
btree_cache_pin_is(void *arg, offset_t offset, uint8_t pin)
{
	struct btree_cache * cache = (struct btree_cache *) arg ;
	return cache_pin_is(cache->cache, offset, pin) ;	
}

int 
btree_cache_rem(void *arg, offset_t offset)
{
	struct btree_cache * cache = (struct btree_cache *) arg ;
	return cache_rem(cache->cache, offset) ;
}

int 
btree_cache_init(void *arg)
{
	struct btree_cache * cache = (struct btree_cache *) arg ;
	cache_init(cache->cache) ;
		
	return 0 ;
}
