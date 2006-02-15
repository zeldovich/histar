#include <inc/error.h>
#include <kern/log.h>
#include <machine/pmap.h>
#include <machine/mmu.h>
#include <machine/x86.h>
#include <machine/stackwrap.h>
#include <lib/btree/btree_traverse.h>

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
	struct btree_volatile disk_map ;

	char just_flushed ;
} ;

// in memory log data
static struct log log ;

static struct
{
#define DLOG_SIZE 8
	uint8_t	 types[DLOG_SIZE] ;
	uint64_t times[DLOG_SIZE] ;

	uint64_t min[3] ;
	uint64_t max[3] ;

	int	ins ;
} dlog ;

enum { flush = 0 , compact, apply } ;
	
static const char *const type_strings[] = {"flush", "compact", "apply"} ;

void 
dlog_init(void)
{
	memset(&dlog, 0, sizeof(dlog)) ;
	
	dlog.min[flush] = ~0 ;
	dlog.min[apply] = ~0 ;
}

static void
dlog_log(uint8_t type, uint64_t start, uint64_t stop)
{
	int i = dlog.ins ;
	dlog.types[i] = type ;
	dlog.times[i] = (stop - start) ;
	if (dlog.min[type] > dlog.times[i])
		dlog.min[type] = dlog.times[i] ;
	if (dlog.max[type] < dlog.times[i])
		dlog.max[type] = dlog.times[i] ;
	dlog.ins = (dlog.ins + 1) % DLOG_SIZE ;
}

void 
dlog_print(void)
{
	cprintf("dlog\n") ;
	
	for (int i  = 0 ; i < DLOG_SIZE ; i++) {	
		if (i == dlog.ins)
			cprintf("--------------------------------\n") ;
		uint8_t type = dlog.types[i] ;
		uint64_t time = dlog.times[i] ;
		cprintf(" %8s\t%ld\n", type_strings[type], time) ;	
	}
	
	cprintf("\n") ;
	cprintf(" %8s+\t%ld\n", type_strings[apply], dlog.max[apply]) ;
	cprintf(" %8s-\t%ld\n", type_strings[apply], dlog.min[apply]) ;
	
	cprintf(" %8s+\t%ld\n", type_strings[flush], dlog.max[flush]) ;
	cprintf(" %8s-\t%ld\n", type_strings[flush], dlog.min[flush]) ;

	uint64_t tot_ap = 0 ;
	uint64_t tot_fl = 0 ;
	
	uint64_t cnt_ap = 0 ;
	uint64_t cnt_fl = 0 ;
	
	for (int i = 0 ; i < DLOG_SIZE ; i++) {
		uint8_t type = dlog.types[i] ;
		uint64_t time = dlog.times[i] ;	
		switch (type) {
			case flush:
				tot_fl += time ;
				cnt_fl++ ;
				break ;
			case apply:
				tot_ap += time ;
				cnt_ap++ ;
				break ;	
		}
	}
	
	if (cnt_ap)
		cprintf(" %8s_ave\t%ld\n", type_strings[apply], tot_ap / cnt_ap) ;
	if (cnt_fl)
		cprintf(" %8s_ave\t%ld\n", type_strings[flush], tot_fl / cnt_fl) ;
	
	cprintf("end\n") ;
}

static int
log_write_list(struct node_list *nodes, uint64_t *count, 
			   offset_t off, struct btree *map)
{
	disk_io_status s ;
	uint64_t n = 0 ;
	
	if (LIST_EMPTY(nodes)) {
		*count = 0 ;
		return 0 ;
	}
		
	struct btree_node *node ;
		
	LIST_FOREACH(node, nodes, node_link) {
		if (off) {
			s = stackwrap_disk_io(op_write, node, PGSIZE, off * PGSIZE);
			if (map)
				btree_insert(map, &node->block.offset, &off) ;
			off++ ;
		}
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

static int
log_read_map(struct btree *map, struct node_list *nodes)
{
	int r ;
	struct btree_node *node ;
	disk_io_status s ;
	
	struct btree_traversal trav ;
	btree_init_traversal(map, &trav) ;
	
	while (btree_next_entry(&trav)) {
		offset_t off = *trav.val ;
			
		if ((r = page_alloc((void **)&node)) < 0)
			return r ;
		s = stackwrap_disk_io(op_read, node, 
							  PGSIZE, off * PGSIZE);
		LIST_INSERT_HEAD(nodes, node, node_link) ;
	}
	
	return 0 ; 
}

static int
log_try_node(offset_t offset, struct btree_node *store)
{
	// when compacting, can have a race between the compaction function
	// and log_node, since compacting modifies disk_map.
	offset_t log_off ;
	if (btree_search(&log.disk_map.tree, &offset, &offset, &log_off) < 0)
		return -E_NOT_FOUND ;
	
	disk_io_status s = stackwrap_disk_io(op_read, store, 
							  			PGSIZE, log_off * PGSIZE);
	if (s != disk_io_success)
		return -E_IO ;

	if (store->block.offset != offset)
		return 1 ;
	
	return 0 ;	
}

int
log_node(offset_t offset, void *page)
{
	
	struct btree_node *node ;
	struct btree_node *store = (struct btree_node *) page ;
	int r ;
	
	LIST_FOREACH(node, &log.nodes, node_link) {
		if (offset == node->block.offset) {
			memcpy(store, node, PGSIZE) ;
			return 0 ;
		}
	}

	// XXX: test
	if (log.on_disk == 0)
		return -E_NOT_FOUND ;
	
	int tries = 0 ;
	// see comment in log_try_node
	while ((r = log_try_node(offset, store)) == 1 && ++tries < 10) ;
		
	if (r < 0)
		return r ;

	if (r == 1)
		panic("log_node: could not read %ld in %d tries\n", offset, tries) ;
		//return -E_UNSPEC ;

	return 0 ;
}


int
log_compact(void)
{
	offset_t off = log.offset + 1 ;
	uint64_t n_nodes = 0 ;
	struct node_list nodes ;
	int r ;
	uint64_t count ;
	
	LIST_INIT(&nodes) ;
	log_read_map(&log.disk_map.tree, &nodes) ;
	if ((r = log_write_list(&nodes, 
							&count, 
							off,
							&log.disk_map.tree)) < 0) {
		log_free_list(&nodes) ;							
		return r ;
	}
		
	n_nodes = count ;
	
	log_free_list(&nodes) ;
	
	// write out a log header
	struct log_header lh = { n_nodes } ;
	memcpy(scratch, &lh, sizeof(lh)) ;
	
	disk_io_status s = stackwrap_disk_io(op_write, scratch, SCRATCH_SIZE, log.offset * PGSIZE);
	if (s != disk_io_success)
		return -E_IO ;
	
	log.on_disk = n_nodes ;
	
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
		memset(store, 0, PGSIZE) ;
		log.in_mem++ ;
	}
		
	memcpy(store, node, BTREE_NODE_SIZE(node->tree->order, node->tree->s_key)) ;
	LIST_INSERT_HEAD(&log.nodes, store, node_link) ;

	log.just_flushed = 0 ;	
	return 0 ;	
}

int
log_flush(void)
{
	int r ;
	disk_io_status s ;
	uint64_t count ;
	
	uint64_t start, stop ;
	start = read_tsc() ;
	
	if (LIST_EMPTY(&log.nodes))
		return 0 ;

	if (log.npages <= log.in_mem + log.on_disk + 1) {
		
		uint64_t d = log.on_disk ;
		if ((r = log_compact()) < 0) {
			cprintf("log_flush: unable to compact: %s\n", e2s(r)) ;
			return r ;	
		}
		cprintf("log_flush: compacted from %ld to %ld\n",d , log.on_disk) ;
	}
	if ((r = log_write_list(&log.nodes, 
							&count, 
							log.offset + log.on_disk + 1,
							&log.disk_map.tree)) < 0)
		return r ;
	assert(count == log.in_mem) ;

	// write out a log header
	struct log_header lh = { log.on_disk + log.in_mem } ;
	memcpy(scratch, &lh, sizeof(lh)) ;

	s = stackwrap_disk_io(op_write, scratch, SCRATCH_SIZE, log.offset * PGSIZE);
	if (s != disk_io_success)
		return -E_IO ;

	log.on_disk += log.in_mem ;

	log.just_flushed = 1 ;

	stop = read_tsc() ;
	dlog_log(flush, start, stop) ;

	return 0 ;
}

static int 
log_apply_disk(offset_t off, uint64_t n_nodes)
{
	int r ;
	struct node_list nodes ;

	if (n_nodes == 0)
		return 0 ;
	
	while (n_nodes) {
		uint64_t n = MIN(n_nodes, log.max_mem) ;
		n_nodes -= n ;
		
		uint64_t count ;
		LIST_INIT(&nodes) ;
		if ((r = log_read_log(off, n, &nodes, &count)) < 0) {
			log_free_list(&nodes) ;
			return r ;			
		}
		
		if ((r = log_write_list(&nodes, &count, 0, 0)) < 0) {
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
	uint64_t start, stop ;
	
	start = read_tsc() ;


	if (log.in_mem != 0) { // have an active in memory log
		if (log.on_disk) {
			offset_t off = log.offset + 1 ;
			uint64_t n_nodes = log.on_disk ;

			if (log.just_flushed)
				n_nodes -= log.in_mem ;	
			
			// XXX: should really be calling something like log_compact...
			if ((r = log_apply_disk(off, n_nodes)) < 0)
				return r ;
		}

		uint64_t count ;
		if ((r = log_write_list(&log.nodes, &count, 0, 0)) < 0)
			return r ;
	
		if (count != log_free_list(&log.nodes))
			panic("log_apply: wrote diff num nodes than in memory?\n") ;
		
		log.in_mem -= count ;
		assert(log.in_mem == 0) ;
				
		log_free() ;
		log_init(log.offset, log.npages, log.max_mem) ;
		
		stop = read_tsc() ;
		
		dlog_log(apply, start, stop) ;
		return 0 ;
	}
	else  { // try to apply log sitting on disk
		disk_io_status s = stackwrap_disk_io(op_read, scratch,
							  SCRATCH_SIZE, log.offset * PGSIZE);
		if (s != disk_io_success)
			return -E_IO ;
			
		struct log_header *lh = (struct log_header *)scratch ;
		uint64_t n_nodes = lh->n_nodes ;
		if (n_nodes == 0)
			return 0 ;
		
		uint64_t off = log.offset + 1 ;
	
		return log_apply_disk(off, n_nodes) ;
	}
}

void
log_print_stats(void)
{
	cprintf("log_print_stats: in_mem %ld on_disk %ld\n", 
			log.in_mem, log.on_disk) ;	
}

void
log_free(void)
{
	btree_erase(&log.disk_map.tree) ;	
}

void
log_init(uint64_t off, uint64_t npages, uint64_t max_mem)
{
	memset(&log, 0, sizeof(log)) ;
	
	// logging will overwrite anything in the disk log
	LIST_INIT(&log.nodes) ;	
	btree_volatile_init(&log.disk_map, BTREE_MAX_ORDER1, 1, 1) ;
	log.offset = off ;
	log.npages = npages ;
	log.max_mem = max_mem ;
}
