#include <btree/btree_manager.h>
#include <btree/btree_debug.h>
#include <btree/btree_impl.h>
#include <btree/btree_node.h>
#include <btree/pbtree.h>
#include <btree/btree.h>
#include <kern/freelist.h>
#include <kern/disklayout.h>
#include <kern/lib.h>
#include <kern/stackwrap.h>
#include <kern/arch.h>
#include <inc/error.h>

// max order for key size of 1
#define BTREE_MAX_ORDER1 252
// max order for key size of 2
#define BTREE_MAX_ORDER2 168

// use to declare a cache for a btree
#define STRUCT_BTREE_CACHE(name, num_ent, order, key_size) \
    STRUCT_CACHE(name, num_ent, BTREE_NODE_SIZE(order, key_size));
#define BTREE_NAME_SIZE 32

struct btree_obj {
    int (*new) (uint64_t id, void **mem, uint64_t * off, void *arg);
    int (*free) (uint64_t id, offset_t offset, void *arg);
    int (*open) (uint64_t id, offset_t offset, void **mem);
    int (*close) (uint64_t id, offset_t offset);
    int (*save) (struct btree_node * node);

    struct btree *btree;
    struct lock lock;
    struct cache *cache;
    char name[BTREE_NAME_SIZE];
    void *arg;
};

#define OBJMAP_NAME     "objmap"
#define IOBJ_NAME       "iobj"
#define FCHUNK_NAME     "freechunks"
#define FOFFSET_NAME    "freeoffsets"

#define OBJMAP_ORDER    BTREE_MAX_ORDER1
#define IOBJ_ORDER      BTREE_MAX_ORDER1
#define FCHUNK_ORDER    BTREE_MAX_ORDER2
#define FOFFSET_ORDER   BTREE_MAX_ORDER1

// should be at least max tree height + 1
#define OBJMAP_CACHE_SIZE   20
#define IOBJ_CACHE_SIZE     20
#define FCHUNK_CACHE_SIZE   20
#define FOFFSET_CACHE_SIZE  20

#define OBJMAP_KEY_SIZE     1
#define IOBJ_KEY_SIZE       1
#define FCHUNK_KEY_SIZE     2
#define FOFFSET_KEY_SIZE    1

#define OBJMAP_VAL_SIZE     2
#define IOBJ_VAL_SIZE       1
#define FCHUNK_VAL_SIZE     1
#define FOFFSET_VAL_SIZE    1

struct btree objmap;
struct btree iobj;
struct btree fchunk;
struct btree foffset;

// XXX: fix...don't have to do this weird static thing...
STRUCT_BTREE_CACHE(objmap_cache, OBJMAP_CACHE_SIZE, OBJMAP_ORDER,
		   OBJMAP_KEY_SIZE);
STRUCT_BTREE_CACHE(iobj_cache, IOBJ_CACHE_SIZE, IOBJ_ORDER, IOBJ_KEY_SIZE);
STRUCT_BTREE_CACHE(fchunk_cache, FCHUNK_CACHE_SIZE, FCHUNK_ORDER,
		   FCHUNK_KEY_SIZE);
STRUCT_BTREE_CACHE(foffset_cache, FOFFSET_CACHE_SIZE, FOFFSET_ORDER,
		   FOFFSET_KEY_SIZE);


static struct btree_obj btree[BTREE_COUNT];

extern struct freelist freelist;

///////////////////////////////
// btree interface
///////////////////////////////

int
btree_alloc_node(uint64_t id, void **mem, uint64_t * off)
{
    if (id >= BTREE_COUNT)
	return -E_INVAL;

    return btree[id].new(id, mem, off, btree[id].arg);
}

int
btree_free_node(uint64_t id, uint64_t off)
{
    if (id >= BTREE_COUNT)
	return -E_INVAL;

    return btree[id].free(id, off, btree[id].arg);
}

int
btree_close_node(uint64_t id, offset_t off)
{
    if (id >= BTREE_COUNT)
	return -E_INVAL;

    return btree[id].close(id, off);
}

int
btree_open_node(uint64_t id, uint64_t off, void **mem)
{
    if (id >= BTREE_COUNT)
	return -E_INVAL;

    return btree[id].open(id, off, mem);
}

int
btree_save_node(uint64_t id, struct btree_node *n)
{
    if (id >= BTREE_COUNT)
	return -E_INVAL;

    return btree[id].save(n);
}

int
btree_refs_node(uint64_t id, uint64_t off)
{
    if (id >= BTREE_COUNT)
	return -E_INVAL;

    return cache_refs(btree[id].cache, off);
}

///////////////////////////////
// client interface
///////////////////////////////

int
btree_search(uint64_t id, const uint64_t * key, uint64_t * key_store,
	     uint64_t * val_store)
{
    return btree_search_impl(btree[id].btree, key, key_store, val_store);
}

int
btree_ltet(uint64_t id, const uint64_t * key, uint64_t * key_store,
	   uint64_t * val_store)
{
    return btree_ltet_impl(btree[id].btree, key, key_store, val_store);
}

int
btree_gtet(uint64_t id, const uint64_t * key, uint64_t * key_store,
	   uint64_t * val_store)
{
    return btree_gtet_impl(btree[id].btree, key, key_store, val_store);
}


char
btree_delete(uint64_t id, const uint64_t * key)
{
    return btree_delete_impl(btree[id].btree, key);
}

int
btree_insert(uint64_t id, const uint64_t * key, offset_t * val)
{
    return btree_insert_impl(btree[id].btree, key, val);
}

int
btree_init_traversal(uint64_t id, struct btree_traversal *trav)
{
    return btree_init_traversal_impl(btree[id].btree, trav);
}

void
btree_pretty_print(uint64_t id)
{
    btree_pretty_print_impl(btree[id].btree);
}

void
btree_sanity_check(uint64_t id)
{
    btree_sanity_check_impl(btree[id].btree);
}

void
btree_lock(uint64_t id)
{
    lock_acquire(&btree[id].lock);
}

void
btree_unlock(uint64_t id)
{
    lock_release(&btree[id].lock);
}

void
btree_lock_all(void)
{
    for (int i = 0; i < BTREE_COUNT; i++)
	btree_lock(i);
}

void
btree_unlock_all(void)
{
    for (int i = 0; i < BTREE_COUNT; i++)
	btree_unlock(i);
}

///////////////////////////////
// manager
///////////////////////////////

struct cache *
btree_cache(uint64_t id)
{
    if (id >= BTREE_COUNT)
	return 0;
    return btree[id].cache;
}

static void
init_ephem(void)
{
    btree[BTREE_OBJMAP].open = pbtree_open_node;
    btree[BTREE_OBJMAP].close = pbtree_close_node;
    btree[BTREE_OBJMAP].save = pbtree_save_node;
    btree[BTREE_OBJMAP].new = pbtree_new_node;
    btree[BTREE_OBJMAP].free = pbtree_free_node;
    btree[BTREE_OBJMAP].btree = &objmap;
    btree[BTREE_OBJMAP].cache = &objmap_cache;
    strncpy(btree[BTREE_OBJMAP].name, OBJMAP_NAME, BTREE_NAME_SIZE);
    cache_init(&objmap_cache);
    lock_init(&btree[BTREE_OBJMAP].lock);

    btree[BTREE_IOBJ].open = pbtree_open_node;
    btree[BTREE_IOBJ].close = pbtree_close_node;
    btree[BTREE_IOBJ].save = pbtree_save_node;
    btree[BTREE_IOBJ].new = pbtree_new_node;
    btree[BTREE_IOBJ].free = pbtree_free_node;
    btree[BTREE_IOBJ].btree = &iobj;
    btree[BTREE_IOBJ].cache = &iobj_cache;
    strncpy(btree[BTREE_IOBJ].name, IOBJ_NAME, BTREE_NAME_SIZE);
    cache_init(&iobj_cache);
    lock_init(&btree[BTREE_IOBJ].lock);

    btree[BTREE_FCHUNK].open = pbtree_open_node;
    btree[BTREE_FCHUNK].close = pbtree_close_node;
    btree[BTREE_FCHUNK].save = pbtree_save_node;
    btree[BTREE_FCHUNK].new = frm_new;
    btree[BTREE_FCHUNK].free = frm_free;
    btree[BTREE_FCHUNK].btree = &fchunk;
    btree[BTREE_FCHUNK].cache = &fchunk_cache;
    btree[BTREE_FCHUNK].arg = &freelist.frm;
    strncpy(btree[BTREE_FCHUNK].name, FCHUNK_NAME, BTREE_NAME_SIZE);
    cache_init(&fchunk_cache);
    lock_init(&btree[BTREE_FCHUNK].lock);

    btree[BTREE_FOFFSET].open = pbtree_open_node;
    btree[BTREE_FOFFSET].close = pbtree_close_node;
    btree[BTREE_FOFFSET].save = pbtree_save_node;
    btree[BTREE_FOFFSET].new = frm_new;
    btree[BTREE_FOFFSET].free = frm_free;
    btree[BTREE_FOFFSET].btree = &foffset;
    btree[BTREE_FOFFSET].cache = &foffset_cache;
    btree[BTREE_FOFFSET].arg = &freelist.frm;
    strncpy(btree[BTREE_FOFFSET].name, FOFFSET_NAME, BTREE_NAME_SIZE);
    cache_init(&foffset_cache);
    lock_init(&btree[BTREE_FOFFSET].lock);

    static_assert(BTREE_NODE_SIZE(IOBJ_ORDER, IOBJ_KEY_SIZE) <=
		  BTREE_BLOCK_SIZE);
    static_assert(BTREE_NODE_SIZE(OBJMAP_ORDER, OBJMAP_KEY_SIZE) <=
		  BTREE_BLOCK_SIZE);
    static_assert(BTREE_NODE_SIZE(FCHUNK_ORDER, FCHUNK_KEY_SIZE) <=
		  BTREE_BLOCK_SIZE);
    static_assert(BTREE_NODE_SIZE(FOFFSET_ORDER, FOFFSET_KEY_SIZE) <=
		  BTREE_BLOCK_SIZE);
}

void
btree_manager_init(void)
{
    init_ephem();

    btree_init_impl(&objmap, BTREE_OBJMAP, OBJMAP_ORDER, OBJMAP_KEY_SIZE,
		    OBJMAP_VAL_SIZE);
    btree_init_impl(&iobj, BTREE_IOBJ, IOBJ_ORDER, IOBJ_KEY_SIZE,
		    IOBJ_VAL_SIZE);
    btree_init_impl(&fchunk, BTREE_FCHUNK, FCHUNK_ORDER, FCHUNK_KEY_SIZE,
		    FCHUNK_VAL_SIZE);
    btree_init_impl(&foffset, BTREE_FOFFSET, FOFFSET_ORDER, FOFFSET_KEY_SIZE,
		    FOFFSET_VAL_SIZE);
}

void
btree_manager_deserialize(void *buf)
{
    init_ephem();

    struct btree *b = (struct btree *) buf;
    for (int i = 0; i < BTREE_COUNT; i++)
	memcpy(btree[i].btree, &b[i], sizeof(struct btree));
}

void
btree_manager_serialize(void *buf)
{
    struct btree *b = (struct btree *) buf;
    for (int i = 0; i < BTREE_COUNT; i++)
	memcpy(&b[i], btree[i].btree, sizeof(struct btree));
}
