#include <inc/error.h>
#include <kern/log.h>
#include <machine/pmap.h>
#include <machine/mmu.h>
#include <machine/stackwrap.h>

#define SCRATCH_SIZE PGSIZE
static uint8_t scratch[SCRATCH_SIZE] ;

struct log_header
{
	uint64_t n_nodes ;
	
	uint64_t offset ;
 	uint64_t npages ;
 	
} ;

static struct log_header log_head ;
static struct node_list nodes ;

void
log_init(uint64_t off, uint64_t npages)
{
	LIST_INIT(&nodes) ;	
	memset(&log_head, 0, sizeof(log_head)) ;
	
	log_head.offset = off ;
	log_head.npages = npages ;
}

int
log_write(struct btree_node *node)
{
	int r ;
	char found = 0 ;	
	struct btree_node *store ;
	
	LIST_FOREACH(store, &nodes, node_link) {
		if (store->block.offset == node->block.offset) {
			LIST_REMOVE(store, node_link) ;
			found = 1 ;
			break ;	
		}
	}
	
	if (!found) {
		if ((r = page_alloc((void **)&store)) < 0)
			return r ;
		log_head.n_nodes++ ;
	}
		
	memcpy(store, node, BTREE_NODE_SIZE(node->tree->order, node->tree->s_key)) ;
	LIST_INSERT_HEAD(&nodes, store, node_link) ;

	return 0 ;	
}

int
log_node(offset_t offset, struct btree_node **store)
{
	struct btree_node *node ;
	
	LIST_FOREACH(node, &nodes, node_link) {
		if (offset == node->block.offset) {
			*store = node ;
			return 0 ;
		}
	}
	*store = 0 ;
	return -E_NOT_FOUND ;	
}

int
log_flush(void)
{
	struct btree_node *node ;
	struct btree_node *prev ;
	
	offset_t off = log_head.offset ;
	uint64_t npages = log_head.npages ;
	
	if (LIST_EMPTY(&nodes))
		return 0 ;

	assert(npages > log_head.n_nodes + 1) ;

	disk_io_status s ;

	memcpy(scratch, &log_head, sizeof(log_head)) ;
	s = stackwrap_disk_io(op_write, scratch, SCRATCH_SIZE, off++ * PGSIZE);
	if (s != disk_io_success)
		return -E_IO ;

	prev = LIST_FIRST(&nodes) ;
	node = LIST_NEXT(prev, node_link) ;

	while(node) {
		memcpy(scratch, prev, BTREE_NODE_SIZE(prev->tree->order, prev->tree->s_key)) ;
		s = stackwrap_disk_io(op_write, scratch, SCRATCH_SIZE, off++ * PGSIZE);
		if (s != disk_io_success)
			return -E_IO ;

		page_free(prev) ;
		log_head.n_nodes-- ;

		prev = node ;
		node = LIST_NEXT(node, node_link) ;
	}
	LIST_INIT(&nodes) ;
	
	memcpy(scratch, prev, BTREE_NODE_SIZE(prev->tree->order, prev->tree->s_key)) ;
	s = stackwrap_disk_io(op_write, scratch, SCRATCH_SIZE, off++ * PGSIZE);
	if (s != disk_io_success)
		return -E_IO ;

	page_free(prev) ;
	log_head.n_nodes-- ;
	
	
	return 0 ;
}

int
log_apply(void)
{
	assert(log_head.n_nodes == 0) ;
	
	offset_t off = log_head.offset ;
	int r ;
	struct btree_node *node ;
	
	disk_io_status s = stackwrap_disk_io(op_read, scratch, 
										 SCRATCH_SIZE, off++ * PGSIZE);
	if (s != disk_io_success)
		return -E_IO ;
		
	struct log_header *lh = (struct log_header *)scratch ;

	if (lh->n_nodes == 0)
		return 0 ;
	
	// read all nodes from log into memory
	for (uint64_t i = 0 ; i < lh->n_nodes ; i++, off++) {
		if ((r = page_alloc((void **)&node)) < 0)
			return r ;
		s = stackwrap_disk_io(op_read, node, 
							  PGSIZE, off * PGSIZE);
		LIST_INSERT_HEAD(&nodes, node, node_link) ;
	}
	
	struct btree_node *prev = LIST_FIRST(&nodes) ;
	node = LIST_NEXT(prev, node_link) ;
	
	// write all nodes to their disk offset
	while(node) {
		s = stackwrap_disk_io(op_write, prev, PGSIZE, prev->block.offset * PGSIZE);
		if (s != disk_io_success)
			return -E_IO ;

		page_free(prev) ;

		prev = node ;
		node = LIST_NEXT(node, node_link) ;
	}
	LIST_INIT(&nodes) ;
	
	s = stackwrap_disk_io(op_write, prev, PGSIZE, prev->block.offset * PGSIZE);
	if (s != disk_io_success)
		return -E_IO ;

	page_free(prev) ;
	
	return 0 ;	
}
