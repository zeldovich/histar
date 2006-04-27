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

static struct {
    uint64_t byteoff;
    uint64_t npages;
    uint64_t max_mem;

    uint64_t in_mem;
    int tailq_inited;
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
    TAILQ_FOREACH(node, nodes, node_log_link) {
	assert(node->block.offset < log.byteoff ||
	       node->block.offset >= log.byteoff + log.npages * PGSIZE);

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
    assert(log.byteoff <= off);

    struct btree_node *node;
    TAILQ_FOREACH(node, nodes, node_log_link) {
#if 0
	if (!node->block.is_dirty)
	    continue;
#endif

	assert(off + n * PGSIZE < log.npages * PGSIZE + log.byteoff);
	s = stackwrap_disk_io(op_write, node, BTREE_BLOCK_SIZE,
			      off + n * PGSIZE);
	if (!SAFE_EQUAL(s, disk_io_success)) {
	    *count = n;
	    return -E_IO;
	}
	hash_put(&log.disk_map, node->block.offset, off + n * PGSIZE);
	node->block.is_dirty = 0;
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

    for (prev = TAILQ_FIRST(nodes); prev; prev = next, n++) {
	next = TAILQ_NEXT(prev, node_log_link);
	TAILQ_REMOVE(nodes, prev, node_log_link);
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
	TAILQ_INSERT_TAIL(nodes, node, node_log_link);
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
	TAILQ_INSERT_HEAD(nodes, node, node_log_link);
    }
    return 0;
}

int
log_try_read(offset_t byteoff, void *page)
{
    struct btree_node *node;
    TAILQ_FOREACH(node, &log.nodes, node_log_link) {
	if (byteoff == node->block.offset) {
	    memcpy(page, node, PGSIZE);
	    return 0;
	}
    }

    // look in on-disk log
    offset_t log_off;
    if (hash_get(&log.disk_map, byteoff, &log_off) < 0)
	return -E_NOT_FOUND;
    disk_io_status s =
	stackwrap_disk_io(op_read, page, BTREE_BLOCK_SIZE, log_off);
    if (!SAFE_EQUAL(s, disk_io_success))
	return -E_IO;

    return 0;
}

int
log_write(struct btree_node *node)
{
    int r;
    char found = 0;
    struct btree_node *store;

    TAILQ_FOREACH(store, &log.nodes, node_log_link) {
	if (store->block.offset == node->block.offset) {
	    TAILQ_REMOVE(&log.nodes, store, node_log_link);
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
    TAILQ_INSERT_TAIL(&log.nodes, store, node_log_link);
    store->block.is_dirty = 1;
    return 0;
}

void
log_free(offset_t byteoff)
{
    struct btree_node *node;
    TAILQ_FOREACH(node, &log.nodes, node_log_link) {
	if (byteoff == node->block.offset) {
	    TAILQ_REMOVE(&log.nodes, node, node_log_link);
	    log.in_mem--;
	    break;
	}
    }
    hash_del(&log.disk_map, byteoff);
}

int64_t
log_flush(void)
{
    int r;

    if (log.npages <= log.in_mem + log.on_disk + 1)
	panic("log_flush: out of log space");

    uint64_t count;
    uint64_t off = log.byteoff + log.on_disk * PGSIZE;
    if ((r = log_write_to_log(&log.nodes, &count, off)) < 0)
	return r;

    assert(count <= log.in_mem);
    log.on_disk += count;

    return log.on_disk;
}

int
log_apply_disk(uint64_t n_nodes)
{
    int r;
    struct node_list nodes;
    uint64_t count, n;
    offset_t off = log.byteoff;

    while (n_nodes) {
	n = MIN(n_nodes, log.max_mem);
	n_nodes -= n;
	TAILQ_INIT(&nodes);
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

    log_init();
    return 0;
}

int
log_apply_mem(void)
{
    int r;
    uint64_t count;
    if ((r = log_write_to_disk(&log.nodes, &count)) < 0)
	return r;

    struct btree_node *node;
    TAILQ_FOREACH(node, &log.nodes, node_log_link)
	hash_del(&log.disk_map, node->block.offset);

    assert(count == log_free_list(&log.nodes));
    log.in_mem -= count;
    assert(log.in_mem == 0);

    if (log.disk_map.size) {
	struct node_list in_log;
	TAILQ_INIT(&in_log);
	log_read_map(&log.disk_map, &in_log);
	log_write_to_disk(&in_log, &count);
	log_free_list(&in_log);
    }

    log_init();
    return 0;
}

void
log_init(void)
{
    if (!log.tailq_inited) {
	TAILQ_INIT(&log.nodes);
	log.tailq_inited = 1;
    }

    log_free_list(&log.nodes);
    memset(&log.map_back[0], 0, sizeof(log.map_back));
    hash_init(&log.disk_map, log.map_back, LOG_PAGES);
    log.in_mem = 0;
    log.on_disk = 0;
    log.byteoff = LOG_OFFSET * PGSIZE;
    log.npages = LOG_PAGES;
    log.max_mem = LOG_MEMORY;
}
