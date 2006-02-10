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
} ;

struct log
{
	uint64_t offset ;
 	uint64_t npages ;
 	uint64_t max_mem ;
 	
	uint64_t in_mem ;
	struct node_list nodes ;

	uint64_t on_disk ;
} ;

// in memory log data
static struct log log ;

int
log_node(offset_t offset, struct btree_node **store)
{
	assert(log.on_disk == 0) ;
	
	struct btree_node *node ;
	
	LIST_FOREACH(node, &log.nodes, node_link) {
		if (offset == node->block.offset) {
			*store = node ;
			return 0 ;
		}
	}
	*store = 0 ;
	return -E_NOT_FOUND ;	
}

static int
log_write_list(struct node_list *nodes, uint64_t *count, offset_t off)
{
	disk_io_status s ;
	uint64_t n = 0 ;
	
	if (LIST_EMPTY(nodes)) {
		*count = 0 ;
		return 0 ;
	}
		
	struct btree_node *node ;
		
	LIST_FOREACH(node, nodes, node_link) {
		if (off)
			s = stackwrap_disk_io(op_write, node, PGSIZE, off++ * PGSIZE);
		else
			s = stackwrap_disk_io(op_write, node, PGSIZE, node->block.offset * PGSIZE);
		if (s != disk_io_success) {
			*count = n ;
			return -E_IO ;
		}
		n++ ;
	}
	*count = n ;
	return 0 ;
}

static uint64_t
log_free_list(struct node_list *nodes)
{
	if (LIST_EMPTY(nodes)) {
		return 0 ;	
	}
	
	uint64_t n = 0 ;
	struct btree_node *prev = LIST_FIRST(nodes) ;
	struct btree_node *node = LIST_NEXT(prev, node_link) ;
	
	while(node) {
		page_free(prev) ;
		prev = node ;
		node = LIST_NEXT(node, node_link) ;
		n++ ;
		
	}
	page_free(prev) ;	
	LIST_INIT(nodes) ;
	n++ ;
	return n ;
}

static int
log_read_log(offset_t off, uint64_t n_nodes, 
			 struct node_list *nodes, uint64_t *count)
{
	int r ;
	struct btree_node *node ;
	disk_io_status s ;
	
	for (uint64_t i = 0 ; i < n_nodes ; i++, off++) {
		if ((r = page_alloc((void **)&node)) < 0)
			return r ;
		s = stackwrap_disk_io(op_read, node, 
							  PGSIZE, off * PGSIZE);
		LIST_INSERT_HEAD(nodes, node, node_link) ;
	}
	return 0 ; 
}

int
log_write(struct btree_node *node)
{
	int r ;
	char found = 0 ;	
	struct btree_node *store ;
	
	LIST_FOREACH(store, &log.nodes, node_link) {
		if (store->block.offset == node->block.offset) {
			LIST_REMOVE(store, node_link) ;
			found = 1 ;
			break ;	
		}
	}
	
	// might put the same node in the log multiple times
	if (!found) {
		if (log.in_mem == log.max_mem) {
			if ((r = log_flush()) < 0)
				return r ;
			uint64_t count = log_free_list(&log.nodes) ;
			log.in_mem -= count ;
			assert(log.in_mem == 0) ;
		}
		
		if ((r = page_alloc((void **)&store)) < 0)
			return r ;
		log.in_mem++ ;
	}
		
	memcpy(store, node, BTREE_NODE_SIZE(node->tree->order, node->tree->s_key)) ;
	LIST_INSERT_HEAD(&log.nodes, store, node_link) ;

	return 0 ;	
}

int
log_flush(void)
{
	int r ;
	disk_io_status s ;
	
	if (LIST_EMPTY(&log.nodes))
		return 0 ;

	if (log.npages <= log.in_mem + log.on_disk + 1)
		panic("log_flush: log overflow, %ld <= %ld + %ld + 1",
			  log.npages, log.in_mem, log.on_disk) ;

	uint64_t count ;
	if ((r = log_write_list(&log.nodes, 
							&count, 
							log.offset + log.on_disk + 1)) < 0)
		return r ;
	assert(count == log.in_mem) ;

	// write out a log header
	struct log_header lh = { log.on_disk + log.in_mem } ;
	memcpy(scratch, &lh, sizeof(lh)) ;

	s = stackwrap_disk_io(op_write, scratch, SCRATCH_SIZE, log.offset * PGSIZE);
	if (s != disk_io_success)
		return -E_IO ;

	log.on_disk += log.in_mem ;

	return 0 ;
}

static int 
log_apply_disk(void)
{
	int r ;
	disk_io_status s ;
	struct node_list nodes ;

	s = stackwrap_disk_io(op_read, scratch,
						  SCRATCH_SIZE, log.offset * PGSIZE);
	if (s != disk_io_success)
		return -E_IO ;
		
	struct log_header *lh = (struct log_header *)scratch ;
	uint64_t n_nodes = lh->n_nodes ;
	if (n_nodes == 0)
		return 0 ;
	
	uint64_t off = log.offset + 1 ;
	
	while (n_nodes) {
		uint64_t n = MIN(n_nodes, log.max_mem) ;
		n_nodes -= n ;
		
		uint64_t count ;
		LIST_INIT(&nodes) ;
		if ((r = log_read_log(off, n, &nodes, &count)) < 0) {
			log_free_list(&nodes) ;
			return r ;			
		}
		
		
		if ((r = log_write_list(&nodes, &count, 0)) < 0) {
			log_free_list(&nodes) ;
			return r ;
		}
		
		if (count != n) {
			log_free_list(&nodes) ;
			return -E_UNSPEC ;
		}
		log_free_list(&nodes) ;
		off += n ;
	}
	return 0 ;
}

int
log_apply(void)
{
	int r ;
	struct btree_node *node ;

	if (log.in_mem != 0) { // have an active in memory log
		if (log.on_disk)
			if ((r = log_apply_disk()) < 0)
				return r ;

		struct btree_node *prev = LIST_FIRST(&log.nodes) ;
		node = LIST_NEXT(prev, node_link) ;
			
		uint64_t count ;
		if ((r = log_write_list(&log.nodes, &count, 0)) < 0)
			return r ;
	
		if (count != log_free_list(&log.nodes))
			panic("log_apply: wrote diff num nodes than in memory?\n") ;
		
		log.in_mem -= count ;
		assert(log.in_mem == 0) ;
		
		log_init(log.offset, log.npages, log.max_mem) ;
		return 0 ;
	}
	else  // try to apply log sitting on disk
		return log_apply_disk() ;
}

void
log_init(uint64_t off, uint64_t npages, uint64_t max_mem)
{
	memset(&log, 0, sizeof(log)) ;
	
	// logging will overwrite anything in the disk log
	LIST_INIT(&log.nodes) ;	
	log.offset = off ;
	log.npages = npages ;
	log.max_mem = max_mem ;
}
