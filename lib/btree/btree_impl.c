#include <lib/btree/btree_impl.h>
#include <lib/btree/btree.h>
#include <lib/btree/cache.h>
#include <machine/stackwrap.h>
#include <machine/mmu.h>
#include <kern/freelist.h>
#include <machine/pmap.h>
#include <kern/lib.h>
#include <kern/log.h>
#include <inc/error.h>
#include <inc/string.h>

#define CENT_NODE(ent) ((struct btree_node *)ent)
#define CENT_CHILDREN(ent) \
	((offset_t *)((uint8_t *)ent + sizeof(struct btree_node)))
#define CENT_KEYS(ent, order) \
	((uint64_t *)((uint8_t *)ent + sizeof(struct btree_node) + \
    sizeof(offset_t) * order))

// buffer for reading and writing nodes to disk
//#define SCRATCH_SIZE PGSIZE
//static uint8_t scratch[SCRATCH_SIZE] ;


/////////////////////
// btree simple
/////////////////////

int 
btree_simple_node(struct btree *tree, 
				 offset_t offset, 
				 struct btree_node **store, 
				 void *man)
{
	struct btree_simple * manager = (struct btree_simple *) man ;
	int r ;	

	if ((r = cache_ent(manager->cache, offset, (uint8_t**) store)) == 0)
		return 0 ;

	uint8_t *buf ;
	r = log_node(offset, (struct btree_node **)&buf) ;
	if (r < 0) {
		if (page_alloc((void**)&buf) < 0)
			return -E_NO_MEM ;
	
		disk_io_status s = 
			stackwrap_disk_io(op_read, 
							  buf, 
							  PGSIZE, 
							  offset * PGSIZE);
		if (s != disk_io_success) {
			cprintf("btree_man_node: error reading node\n");
			page_free(buf) ;
			*store = 0 ;
			return -1;
		}
	}
	
	r = cache_try_insert(manager->cache, offset, 
						buf, (uint8_t **)store) ;

	page_free(buf)	 ;

	if (r < 0) {
		*store = 0 ;
		return r ;
	}

	struct btree_node *node = *store ;

	// setup pointers in node
	node->block.offset = offset ;
	node->tree = tree ;

	node->children = CENT_CHILDREN(node) ;
	node->keys = CENT_KEYS(node, manager->order) ;
	node->block.offset = offset ;
	node->tree = tree ;
					
	return 0 ;
}

int 
btree_simple_write(struct btree_node *node, void *manager __attribute__((unused)))
{
	/*
	struct btree *tree = node->tree ;
	memcpy(scratch, node, BTREE_NODE_SIZE(tree->order, tree->s_key)) ;

	disk_io_status s = 
		stackwrap_disk_io(op_write, 
						  scratch, 
						  SCRATCH_SIZE, 
						  node->block.offset * PGSIZE);

	if (s != disk_io_success) {
		cprintf("btree_man_write: error writing node\n");
		return -1;
	}*/
	
	log_write(node) ;
	return 0;
}

int 
btree_simple_alloc(struct btree *tree, 
			  	  offset_t offset, 
			  	  struct btree_node **store, 
			  	  void *man)
{
	struct btree_simple *manager = (struct btree_simple *) man ;
	struct btree_node *node ;
	
	int 	r ;	
	uint8_t *buf ;

	if ((r = cache_alloc(manager->cache, offset, &buf)) < 0) {
		cprintf("btree_man_alloc: cache fully pinned (%d)\n", manager->cache->n_ent) ;
		*store = 0 ;
		return -E_NO_SPACE ;
	}
	
	node = CENT_NODE(buf) ;
	memset(node, 0, sizeof(struct btree_node)) ;

	// setup pointers in node
	node->children = CENT_CHILDREN(buf) ;
	node->keys = CENT_KEYS(buf, manager->order) ;
	node->block.offset = offset ;
	node->tree = tree ;

	*store = node ;
	return 0 ;
}

int 
btree_simple_rem(void *man, offset_t offset)
{
	struct btree_simple * manager = (struct btree_simple *) man ;
	return cache_rem(manager->cache, offset) ;
}

int 
btree_simple_unpin(void *man)
{
	struct btree_simple * manager = (struct btree_simple *) man ;
	return cache_unpin(manager->cache) ;
}

int 
btree_simple_init(struct btree_simple *sim, uint8_t order, 
				 struct cache *cache)
{
	sim->cache = cache ;
	sim->order = order ;
	cache_init(sim->cache) ;
		
	return 0 ;
}

/////////////////////
// btree default
/////////////////////

static int
btree_default_alloc(struct btree *tree, struct btree_node **store, void *arg)
{
	struct btree_default *def = (struct btree_default *) arg ;
	int64_t off = freelist_alloc(def->fl, 1) ;
	if (off < 0)
		return off ;
	
	return btree_simple_alloc(tree, off, store, &def->simple) ;
}

int 
btree_default_setup(struct btree_default *def, uint8_t order,
				    struct freelist *fl,struct cache *cache)
{
	btree_simple_init(&def->simple, order, cache) ;

	def->fl = fl ;
	
	def->tree.manager.alloc = &btree_default_alloc ;
	def->tree.manager.free = &btree_simple_rem ;
	def->tree.manager.node = &btree_simple_node ;
	def->tree.manager.arg = def ;
	def->tree.manager.unpin = &btree_simple_unpin ;
	def->tree.manager.write = &btree_simple_write ;
	   	
	return 0 ;				
}

int 
btree_default_init(struct btree_default *def, uint8_t order, 
				   uint8_t key_size, uint8_t value_size,
				   struct freelist *fl,struct cache *cache)
{
	btree_init(&def->tree, order, key_size, value_size, NULL) ;
	btree_default_setup(def, order, fl, cache) ;
	   	
	return 0 ;
}
