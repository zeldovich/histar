#include <inc/error.h>
#include <kern/log.h>
#include <kern/disklayout.h>
#include <machine/pmap.h>
#include <machine/mmu.h>
#include <machine/x86.h>
#include <machine/stackwrap.h>
#include <lib/btree/btree_traverse.h>
#include <lib/hashtable.h>

#define SCRATCH_SIZE PGSIZE
static uint8_t scratch[SCRATCH_SIZE] ;

struct log_header
{
	uint64_t n_nodes ;
} ;

struct log
{
	uint64_t byteoff ;
 	uint64_t npages ;
 	uint64_t max_mem ;
 	
	uint64_t in_mem ;
	struct node_list nodes ;

	uint64_t on_disk ;
    struct hashtable disk_map2 ;
    struct hashentry map_back[LOG_SIZE] ;

	uint64_t log_gen ;
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
log_write_to_disk(struct node_list *nodes, uint64_t *count)
{
	disk_io_status s ;
	uint64_t n = 0 ;
	
	if (LIST_EMPTY(nodes)) {
		*count = 0 ;
		return 0 ;
	}
		
	struct btree_node *node ;
		
	LIST_FOREACH(node, nodes, node_link) {
		s = stackwrap_disk_io(op_write, node, BTREE_BLOCK_SIZE, node->block.offset);
		
		if (!SAFE_EQUAL(s, disk_io_success)) {
			*count = n ;
			return -E_IO ;
		}
		n++ ;
	}
	*count = n ;
	return 0 ;
}

static int
log_write_to_log(struct node_list *nodes, uint64_t *count, offset_t off)
{
	disk_io_status s ;
	uint64_t n = 0 ;
	
	if (log.byteoff > off || log.npages * PGSIZE + log.byteoff <= off)
		return -E_INVAL ;
	
	if (LIST_EMPTY(nodes)) {
		*count = 0 ;
		return 0 ;
	}
		
	struct btree_node *node ;
		
	LIST_FOREACH(node, nodes, node_link) {
		s = stackwrap_disk_io(op_write, node, BTREE_BLOCK_SIZE, off);
		log.log_gen++ ;
		if (!SAFE_EQUAL(s, disk_io_success)) {
			*count = n ;
			return -E_IO ;
		}

        hash_put(&log.disk_map2, node->block.offset, off) ;
		off += PGSIZE ;
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
	
	for (uint64_t i = 0 ; i < n_nodes ; i++, off += BTREE_BLOCK_SIZE) {
		if ((r = page_alloc((void **)&node)) < 0)
			return r ;
		s = stackwrap_disk_io(op_read, node, BTREE_BLOCK_SIZE, off);
		LIST_INSERT_HEAD(nodes, node, node_link) ;
	}
	return 0 ; 
}

static int
log_read_map2(struct hashtable *map, struct node_list *nodes)
{
    int r ;
    struct btree_node *node ;
    disk_io_status s ;
    
    struct hashiter iter ;
    hashiter_init(map, &iter) ;
    
    while (hashiter_next(&iter)) {
        offset_t off = iter.hi_val ;
            
        if ((r = page_alloc((void **)&node)) < 0)
            return r ;
        s = stackwrap_disk_io(op_read, node, BTREE_BLOCK_SIZE, off);
        assert(node->block.offset == iter.hi_key) ;
        LIST_INSERT_HEAD(nodes, node, node_link) ;
    }
    return 0 ; 
}

static int
log_try_node(offset_t offset, struct btree_node *store)
{
	// compacting modifies disk_map - can have a race w/ log_node
    offset_t log_off ;
	
    if (hash_get(&log.disk_map2, offset, &log_off) < 0)
       return -E_NOT_FOUND ;
    
	uint64_t gen = log.log_gen ;
	
	disk_io_status s = stackwrap_disk_io(op_read, store, BTREE_BLOCK_SIZE, log_off);
	if (!SAFE_EQUAL(s, disk_io_success))
		return -E_IO ;

	// compacting modified disk_map
	if (store->block.offset != offset || gen != log.log_gen)
		return 1 ;
	
	return 0 ;	
}

int
log_node(offset_t byteoff, void *page)
{
	
	struct btree_node *node ;
	struct btree_node *store = (struct btree_node *) page ;
	int r ;
	
	LIST_FOREACH(node, &log.nodes, node_link) {
		if (byteoff == node->block.offset) {
			memcpy(store, node, PGSIZE) ;
			return 0 ;
		}
	}

	// XXX: test
	if (log.on_disk == 0)
		return -E_NOT_FOUND ;
	
	int tries = 0 ;
	// see comment in log_try_node
	while ((r = log_try_node(byteoff, store)) == 1 && ++tries < 10) ;
		
	if (r < 0)
		return r ;

	if (r == 1)
		panic("log_node: could not read %ld in %d tries\n", byteoff, tries) ;
		//return -E_UNSPEC ;

	return 0 ;
}

#if 0
static char
lists_equal(struct node_list *l1, struct node_list *l2)
{
    struct btree_node *n1 ;
    struct btree_node *n2 ;
    
    
    
    LIST_FOREACH(n1, l1, node_link) {
        char found = 0 ;
        LIST_FOREACH(n2, l2, node_link) {
            if (n1->block.offset == n2->block.offset) {
                found = 1 ;
                break ;   
            }
                
        }
        if (!found)
            return 0 ;
    }
    return 1 ;
}
#endif

int
log_compact(void)
{
	offset_t off = log.byteoff + PGSIZE ;
	uint64_t n_nodes = 0 ;
    struct node_list nodes ;
	int r ;
	uint64_t count ;
	
    LIST_INIT(&nodes) ;
	
    log_read_map2(&log.disk_map2, &nodes) ;
	
	if ((r = log_write_to_log(&nodes, &count,off)) < 0) {
		log_free_list(&nodes) ;							
		return r ;
	}
		
	n_nodes = count ;
	
	log_free_list(&nodes) ;
	
	// write out a log header
	struct log_header lh = { n_nodes } ;
	memcpy(scratch, &lh, sizeof(lh)) ;
	
	disk_io_status s = stackwrap_disk_io(op_write, scratch, SCRATCH_SIZE, log.byteoff);
	log.log_gen++ ;
	if (!SAFE_EQUAL(s, disk_io_success))
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

void
log_free(offset_t byteoff)
{
    struct btree_node *node ;
    
    LIST_FOREACH(node, &log.nodes, node_link) {
        if (byteoff == node->block.offset) {
            LIST_REMOVE(node, node_link) ;
            log.in_mem-- ;
            break ;
        }
    }
    hash_del(&log.disk_map2, byteoff) ;
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
		uint64_t needed = log.in_mem + log.on_disk + 1 ;
		if (log.npages <= needed)
			panic("log_flush: log overflow, %ld <= %ld", log.npages, needed) ;
	}
	uint64_t off = log.byteoff + (log.on_disk + 1) * PGSIZE ;
	if ((r = log_write_to_log(&log.nodes, &count, off)) < 0)
		return r ;
	assert(count == log.in_mem) ;

	// write out a log header
	struct log_header lh = { log.on_disk + log.in_mem } ;
	memcpy(scratch, &lh, sizeof(lh)) ;

	s = stackwrap_disk_io(op_write, scratch, SCRATCH_SIZE, log.byteoff);
	log.log_gen++ ;
	if (!SAFE_EQUAL(s, disk_io_success))
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
		
		if ((r = log_write_to_disk(&nodes, &count)) < 0) {
			log_free_list(&nodes) ;
			return r ;
		}
		
		if (count != n) {
			log_free_list(&nodes) ;
			return -E_UNSPEC ;
		}
		log_free_list(&nodes) ;
		off += n * PGSIZE ;
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
		uint64_t count ;
		if ((r = log_write_to_disk(&log.nodes, &count)) < 0)
			return r ;
	
		struct btree_node *node ;

        LIST_FOREACH(node, &log.nodes, node_link)
            hash_del(&log.disk_map2, node->block.offset) ;

		if (count != log_free_list(&log.nodes))
			panic("log_apply: wrote diff num nodes than in memory?\n") ;

		log.in_mem -= count ;
		assert(log.in_mem == 0) ;
		
        
        if (log.disk_map2.size) {
            struct node_list in_log ;		
			LIST_INIT(&in_log) ;
			log_read_map2(&log.disk_map2, &in_log) ;
			log_write_to_disk(&in_log, &count) ;
			log_free_list(&in_log) ;
		}
				
		log_reset() ;
		stop = read_tsc() ;
		dlog_log(apply, start, stop) ;
		return 0 ;
	}
	else  { // try to apply log sitting on disk
		disk_io_status s = stackwrap_disk_io(op_read, scratch,
							  SCRATCH_SIZE, log.byteoff);
		if (!SAFE_EQUAL(s, disk_io_success))
			return -E_IO ;
			
		struct log_header *lh = (struct log_header *)scratch ;
		uint64_t n_nodes = lh->n_nodes ;
		if (n_nodes == 0)
			return 0 ;
		
		uint64_t off = log.byteoff + PGSIZE ;
	
		return log_apply_disk(off, n_nodes) ;
	}
}

void
log_reset(void)
{
	log_init(log.byteoff / PGSIZE, log.npages, log.max_mem) ;
}

void
log_init(uint64_t pageoff, uint64_t npages, uint64_t max_mem)
{
	memset(&log, 0, sizeof(log)) ;
	
	// logging will overwrite anything in the disk log
	LIST_INIT(&log.nodes) ;	
	hash_init(&log.disk_map2, log.map_back, LOG_SIZE) ;
	log.byteoff = pageoff * PGSIZE;
	log.npages = npages ;
	log.max_mem = max_mem ;
}
