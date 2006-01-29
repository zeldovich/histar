#include <lib/btree/btree_man.h>
#include <lib/btree/btree.h>
#include <lib/btree/cache.h>
#include <machine/stackwrap.h>
#include <machine/mmu.h>
#include <kern/lib.h>
#include <inc/error.h>
#include <inc/string.h>

#define CENT_NODE(ent) ((struct btree_node *)ent)
#define CENT_CHILDREN(ent) \
	((offset_t *)((uint8_t *)ent + sizeof(struct btree_node)))
#define CENT_KEYS(ent, order) \
	((uint64_t *)((uint8_t *)ent + sizeof(struct btree_node) + \
    sizeof(offset_t) * order))

// don't want to change size of btree_node
struct pair {
	struct btree_node *node ;
	char done ;	
} ;

// buffer for reading and writing nodes to disk
#define SCRATCH_SIZE PGSIZE
static uint8_t scratch[SCRATCH_SIZE] ;

static void
read_node_stackwrap(void *arg)
{
	struct pair *p = (struct pair *)arg ;
	struct btree_node *node = p->node ;
	struct btree *tree = node->tree ;
	
	disk_io_status s = 
		stackwrap_disk_io(op_read, 
						  scratch, 
						  SCRATCH_SIZE, 
						  node->block.offset * PGSIZE);
	
    if (s != disk_io_success)
		cprintf("read_node_stackwrap: error reading node\n");
	else
		memcpy(node, scratch, BTREE_NODE_SIZE(tree->order, tree->s_key)) ;
	
	p->done = 1 ;
}

int 
btree_man_node(struct btree *tree, 
				 offset_t offset, 
				 struct btree_node **store, 
				 void *man)
{
	struct btree_man * manager = (struct btree_man *) man ;
	int r ;	

	if ((r = cache_ent(manager->cache, offset, (uint8_t**) store)) == 0)
		return 0 ;

	// XXX: needs eviction/replacement
	uint8_t *buf ;
	if ((r = cache_alloc(manager->cache, offset, &buf)) < 0) {
		*store = 0 ;
		return -E_NO_SPACE ;
	}

	// read node off disk
	struct btree_node *node = node = CENT_NODE(buf) ;
	node->block.offset = offset ;
	node->tree = tree ;
	struct pair p = { node, 0 } ;

	r = stackwrap_call(&read_node_stackwrap, &p);
    if (r < 0) {
		cprintf("btree_man_node: cannot stackwrap: %s\n", e2s(r));
    	*store = 0 ;
    	return r ;
    }
	
	while (!p.done) 
		ide_intr() ;

	// setup pointers in node
	node->children = CENT_CHILDREN(buf) ;
	node->keys = CENT_KEYS(buf, manager->order) ;
	node->block.offset = offset ;
	node->tree = tree ;
	*store = node ;
	
	return 0 ;
}

static void
write_node_stackwrap(void *arg)
{
	struct pair *p = (struct pair *)arg ;
	struct btree_node *node = p->node ;
	struct btree *tree = node->tree ;
	
	memcpy(scratch, node, BTREE_NODE_SIZE(tree->order, tree->s_key)) ;
	
	disk_io_status s = 
		stackwrap_disk_io(op_write, 
						  scratch, 
						  SCRATCH_SIZE, 
						  node->block.offset * PGSIZE);

    if (s != disk_io_success)
		cprintf("write_node_stackwrap: error writing node\n");
	
	p->done = 1 ;
}

int 
btree_man_write(struct btree_node *node, void *manager)
{
	struct pair p = { node, 0 } ;
	
	int r = stackwrap_call(&write_node_stackwrap, &p);
    if (r < 0)
		cprintf("btree_man_write: cannot stackwrap: %s\n", e2s(r));
	
	while (!p.done) 
		ide_intr() ;
	
	return r ;
}

int 
btree_man_alloc(struct btree *tree, 
			  	  offset_t offset, 
			  	  struct btree_node **store, 
			  	  void *man)
{
	struct btree_man *manager = (struct btree_man *) man ;
	struct btree_node *node ;
	
	int 	r ;	
	uint8_t *buf ;

	if ((r = cache_alloc(manager->cache, offset, &buf)) < 0) {
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
btree_man_pin_is(void *man, offset_t offset, uint8_t pin)
{
	struct btree_man * manager = (struct btree_man *) man ;
	return cache_pin_is(manager->cache, offset, pin) ;	
}

int 
btree_man_rem(void *man, offset_t offset)
{
	struct btree_man * manager = (struct btree_man *) man ;
	return cache_rem(manager->cache, offset) ;
}

int 
btree_man_init(void *man)
{
	struct btree_man * manager = (struct btree_man *) man ;
	cache_init(manager->cache) ;
		
	return 0 ;
}
