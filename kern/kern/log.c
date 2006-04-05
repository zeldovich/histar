#include <machine/pmap.h>
#include <machine/mmu.h>
#include <machine/x86.h>
#include <machine/stackwrap.h>
#include <kern/log.h>
#include <kern/disklayout.h>
#include <kern/freelist.h>
#include <kern/hashtable.h>
#include <kern/disklayout.h>
#include <inc/queue.h>
#include <inc/error.h>

#define SCRATCH_SIZE PGSIZE
static uint8_t scratch[SCRATCH_SIZE];

struct log_header {
    uint64_t n_nodes;
};

static struct {
    uint64_t byteoff;
    uint64_t npages;
    uint64_t max_mem;

    uint64_t in_mem;
    struct node_list nodes;

    uint64_t on_disk;
    struct hashtable disk_map;
    struct hashentry map_back[LOG_PAGES];
} log;

static int
log_write_to_disk(struct node_list *nodes, uint64_t * count)
{
    disk_io_status s;
    uint64_t n = 0;

    struct btree_node *node;
    LIST_FOREACH(node, nodes, node_link) {
	s = stackwrap_disk_io(op_write, node, BTREE_BLOCK_SIZE,
			      node->block.offset);
	if (!SAFE_EQUAL(s, disk_io_success)) {
	    *count = n;
	    return -E_IO;
	}
	n++;
    }
    *count = n;
    return 0;
}

static int
log_write_to_log(struct node_list *nodes, uint64_t * count, offset_t off)
{
    disk_io_status s;
    uint64_t n = 0;
    assert(log.byteoff < off);

    struct btree_node *node;
    LIST_FOREACH(node, nodes, node_link) {
	assert(off + n * PGSIZE < log.npages * PGSIZE + log.byteoff);
	s = stackwrap_disk_io(op_write, node, BTREE_BLOCK_SIZE,
			      off + n * PGSIZE);
	if (!SAFE_EQUAL(s, disk_io_success)) {
	    *count = n;
	    return -E_IO;
	}
	hash_put(&log.disk_map, node->block.offset, off + n * PGSIZE);
	n++;
    }
    *count = n;
    return 0;
}

static uint64_t
log_free_list(struct node_list *nodes)
{
    uint64_t n = 0;
    struct btree_node *prev, *next;

    for (prev = LIST_FIRST(nodes); prev; prev = next, n++) {
	next = LIST_NEXT(prev, node_link);
	LIST_REMOVE(prev, node_link);
	page_free(prev);
    }
    return n;
}

static int
log_read_log(offset_t off, uint64_t n_nodes, struct node_list *nodes)
{
    int r;
    struct btree_node *node;
    disk_io_status s;

    for (uint64_t i = 0; i < n_nodes; i++, off += BTREE_BLOCK_SIZE) {
	if ((r = page_alloc((void **) &node)) < 0)
	    return r;
	s = stackwrap_disk_io(op_read, node, BTREE_BLOCK_SIZE, off);
	if (!SAFE_EQUAL(s, disk_io_success))
	    return -E_IO;
	LIST_INSERT_HEAD(nodes, node, node_link);
    }
    return 0;
}

static int
log_read_map(struct hashtable *map, struct node_list *nodes)
{
    int r;
    struct btree_node *node;
    disk_io_status s;
    struct hashiter iter;

    for (hashiter_init(map, &iter); hashiter_next(&iter);) {
	offset_t off = iter.hi_val;
	if ((r = page_alloc((void **) &node)) < 0)
	    return r;
	s = stackwrap_disk_io(op_read, node, BTREE_BLOCK_SIZE, off);
	if (!SAFE_EQUAL(s, disk_io_success))
	    return -E_IO;
	assert(node->block.offset == iter.hi_key);
	LIST_INSERT_HEAD(nodes, node, node_link);
    }
    return 0;
}

int
log_try_read(offset_t byteoff, void *page)
{
    struct btree_node *node;
    struct btree_node *store = (struct btree_node *) page;
    LIST_FOREACH(node, &log.nodes, node_link) {
	if (byteoff == node->block.offset) {
	    memcpy(store, node, PGSIZE);
	    return 0;
	}
    }

    // look in on-disk log
    offset_t log_off;
    if (hash_get(&log.disk_map, byteoff, &log_off) < 0)
	return -E_NOT_FOUND;
    disk_io_status s =
	stackwrap_disk_io(op_read, store, BTREE_BLOCK_SIZE, log_off);
    if (!SAFE_EQUAL(s, disk_io_success))
	return -E_IO;

    return 0;
}

static int
log_compact(void)
{
    offset_t off = log.byteoff + PGSIZE;
    struct node_list nodes;
    uint64_t count = 0;
    int r;

    LIST_INIT(&nodes);
    // XXX: potentially a lot of nodes read from disk
    log_read_map(&log.disk_map, &nodes);
    r = log_write_to_log(&nodes, &count, off);
    log_free_list(&nodes);
    if (r < 0)
	return r;

    // write out a log header
    struct log_header lh = { count };
    memcpy(scratch, &lh, sizeof(lh));
    disk_io_status s =
	stackwrap_disk_io(op_write, scratch, SCRATCH_SIZE, log.byteoff);
    if (!SAFE_EQUAL(s, disk_io_success))
	return -E_IO;

    log.on_disk = count;
    return 0;
}

int
log_write(struct btree_node *node)
{
    int r;
    char found = 0;
    struct btree_node *store;

    LIST_FOREACH(store, &log.nodes, node_link) {
	if (store->block.offset == node->block.offset) {
	    LIST_REMOVE(store, node_link);
	    found = 1;
	    break;
	}
    }
    if (!found) {
	if (log.in_mem == log.max_mem) {
	    if ((r = log_flush()) < 0)
		return r;
	    uint64_t count = log_free_list(&log.nodes);
	    log.in_mem -= count;
	    assert(log.in_mem == 0);
	}
	if ((r = page_alloc((void **) &store)) < 0)
	    return r;
	memset(store, 0, PGSIZE);
	log.in_mem++;
    }

    memcpy(store, node,
	   BTREE_NODE_SIZE(node->tree->order, node->tree->s_key));
    LIST_INSERT_HEAD(&log.nodes, store, node_link);
    return 0;
}

void
log_free(offset_t byteoff)
{
    struct btree_node *node;
    LIST_FOREACH(node, &log.nodes, node_link) {
	if (byteoff == node->block.offset) {
	    LIST_REMOVE(node, node_link);
	    log.in_mem--;
	    break;
	}
    }
    hash_del(&log.disk_map, byteoff);
}

int
log_flush(void)
{
    int r;
    disk_io_status s;

    if (LIST_EMPTY(&log.nodes))
	return 0;
    if (log.npages <= log.in_mem + log.on_disk + 1) {
	uint64_t d = log.on_disk;
	if ((r = log_compact()) < 0) {
	    cprintf("log_flush: unable to compact: %s\n", e2s(r));
	    return r;
	}
	cprintf("log_flush: compacted from %ld to %ld\n", d, log.on_disk);
	uint64_t needed = log.in_mem + log.on_disk + 1;
	assert(log.npages >= needed);
    }
    uint64_t count;
    uint64_t off = log.byteoff + (log.on_disk + 1) * PGSIZE;
    if ((r = log_write_to_log(&log.nodes, &count, off)) < 0)
	return r;
    assert(count == log.in_mem);

    // write out a log header
    log.on_disk += log.in_mem;
    struct log_header lh = { log.on_disk };
    memcpy(scratch, &lh, sizeof(lh));
    s = stackwrap_disk_io(op_write, scratch, SCRATCH_SIZE, log.byteoff);
    if (!SAFE_EQUAL(s, disk_io_success))
	return -E_IO;
    return 0;
}

static int
log_apply_disk(offset_t off, uint64_t n_nodes)
{
    int r;
    struct node_list nodes;
    uint64_t count, n;

    while (n_nodes) {
	n = MIN(n_nodes, log.max_mem);
	n_nodes -= n;
	LIST_INIT(&nodes);
	if ((r = log_read_log(off, n, &nodes)) < 0) {
	    log_free_list(&nodes);
	    return r;
	}
	if ((r = log_write_to_disk(&nodes, &count)) < 0) {
	    log_free_list(&nodes);
	    return r;
	}
	assert(count == n);
	log_free_list(&nodes);
	off += n * PGSIZE;
    }
    return 0;
}

int
log_apply(void)
{
    int r;
    if (log.in_mem != 0) {	// have an active in memory log
	uint64_t count;
	if ((r = log_write_to_disk(&log.nodes, &count)) < 0)
	    return r;

	struct btree_node *node;
	LIST_FOREACH(node, &log.nodes, node_link)
	    hash_del(&log.disk_map, node->block.offset);

	assert(count == log_free_list(&log.nodes));
	log.in_mem -= count;
	assert(log.in_mem == 0);

	if (log.disk_map.size) {
	    struct node_list in_log;
	    LIST_INIT(&in_log);
	    log_read_map(&log.disk_map, &in_log);
	    log_write_to_disk(&in_log, &count);
	    log_free_list(&in_log);
	}
	log_init();
	return 0;
    } else {			// try to apply log sitting on disk
	disk_io_status s = stackwrap_disk_io(op_read, scratch,
					     SCRATCH_SIZE, log.byteoff);
	if (!SAFE_EQUAL(s, disk_io_success))
	    return -E_IO;
	struct log_header *lh = (struct log_header *) scratch;
	uint64_t n_nodes = lh->n_nodes;
	if (n_nodes == 0)
	    return 0;
	uint64_t off = log.byteoff + PGSIZE;
	return log_apply_disk(off, n_nodes);
    }
}

void
log_init(void)
{
    memset(&log, 0, sizeof(log));
    // logging will overwrite anything in the disk log
    LIST_INIT(&log.nodes);
    hash_init(&log.disk_map, log.map_back, LOG_PAGES);
    log.byteoff = LOG_OFFSET * PGSIZE;
    log.npages = LOG_PAGES;
    log.max_mem = LOG_MEMORY;
}
