#include <lib/btree/btree_impl.h>
#include <lib/btree/btree.h>
#include <lib/btree/cache.h>
#include <machine/stackwrap.h>
#include <machine/mmu.h>
#include <kern/freelist.h>
#include <kern/lib.h>
#include <inc/error.h>
#include <inc/string.h>

#define CENT_NODE(ent) ((struct btree_node *)ent)
#define CENT_CHILDREN(ent) \
	((offset_t *)((uint8_t *)ent + sizeof(struct btree_node)))
#define CENT_KEYS(ent, order) \
	((uint64_t *)((uint8_t *)ent + sizeof(struct btree_node) + \
    sizeof(offset_t) * order))

// buffer for reading and writing nodes to disk
#define SCRATCH_SIZE PGSIZE
static uint8_t scratch[SCRATCH_SIZE] ;


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
	if ((r = cache_alloc(manager->cache, offset, &buf)) < 0) {
		cprintf("btree_man_node: cache fully pinned (%d)\n", manager->cache->n_ent) ;
		*store = 0 ;
		return -E_NO_SPACE ;
	}

	// read node off disk
	struct btree_node *node = node = CENT_NODE(buf) ;
	node->block.offset = offset ;
	node->tree = tree ;

	disk_io_status s = 
		stackwrap_disk_io(op_read, 
						  scratch, 
						  SCRATCH_SIZE, 
						  node->block.offset * PGSIZE);
	if (s != disk_io_success) {
		cprintf("btree_man_node: error reading node\n");
		*store = 0 ;
		return -1;
	}

	memcpy(node, scratch, BTREE_NODE_SIZE(tree->order, tree->s_key)) ;

	// setup pointers in node
	node->children = CENT_CHILDREN(buf) ;
	node->keys = CENT_KEYS(buf, manager->order) ;
	node->block.offset = offset ;
	node->tree = tree ;
	*store = node ;
	
	return 0 ;
}

int 
btree_simple_write(struct btree_node *node, void *manager)
{
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
	}

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
	offset_t off = freelist_alloc(def->fl, 1) ;
	if (off < 0)
		return off ;
	
	return btree_simple_alloc(tree, off, store, &def->simple) ;
}

int 
btree_default_setup(struct btree_default *def, uint8_t order, uint8_t key_size,
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
btree_default_init(struct btree_default *def, uint8_t order, uint8_t key_size,
				   struct freelist *fl,struct cache *cache)
{
	btree_init(&def->tree, order, key_size, NULL) ;
	btree_default_setup(def, order, key_size, fl, cache) ;
	   	
	return 0 ;
}
