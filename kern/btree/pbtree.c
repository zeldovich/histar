#include <btree/pbtree.h>
#include <btree/btree_manager.h>
#include <btree/cache.h>
#include <btree/btree.h>
#include <kern/freelist.h>
#include <kern/stackwrap.h>
#include <kern/log.h>
#include <kern/arch.h>
#include <kern/pstate.h>
#include <inc/error.h>

extern struct freelist freelist;

int
pbtree_open_node(uint64_t id, offset_t offset, void ** mem)
{
    int r;
    struct cache *cache = btree_cache(id);
    if ((r = cache_ent(cache, offset, mem)) == 0)
	return 0;

    static struct lock buffer_lock;
    static uint8_t buf[BTREE_BLOCK_SIZE];

    lock_acquire(&buffer_lock);

    r = log_try_read(offset, &buf[0]);
    if (r < 0) {
	// 'updated' node not in log, so read from disk
	disk_io_status s = stackwrap_disk_io(op_read,
					     pstate_part,
					     &buf[0],
					     BTREE_BLOCK_SIZE,
					     offset);
	if (!SAFE_EQUAL(s, disk_io_success)) {
	    cprintf("btree_simple_node: error reading node from disk\n");
	    lock_release(&buffer_lock);
	    *mem = 0;
	    return -E_IO;
	}
    }

    r = cache_try_insert(cache, offset, buf, mem);
    lock_release(&buffer_lock);

    if (r < 0) {
	*mem = 0;
	return r;
    }

    return 0;
}

int
pbtree_close_node(uint64_t id, offset_t offset)
{
    return cache_dec_ref(btree_cache(id), offset);
}

int
pbtree_save_node(struct btree_node *node)
{
    return log_write(node);
}

int
pbtree_new_node(uint64_t id, void **mem, uint64_t * off, void *arg)
{
    int64_t offset = freelist_alloc(&freelist, BTREE_BLOCK_SIZE);

    if (offset < 0)
	return offset;

    void *buf;
    if ((cache_alloc(btree_cache(id), offset, &buf)) < 0) {
	cprintf("new: cache fully pinned (%d)\n", btree_cache(id)->n_ent);
	*mem = 0;
	*off = 0;
	return -E_NO_SPACE;
    }

    *mem = buf;
    *off = offset;
    return 0;
}

int
pbtree_free_node(uint64_t id, offset_t offset, void *arg)
{
    freelist_free_later(&freelist, offset, BTREE_BLOCK_SIZE);
    log_free(offset);
    return cache_rem(btree_cache(id), offset);
}
