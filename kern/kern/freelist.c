#include <machine/mmu.h>
#include <kern/freelist.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/stdio.h>
#include <inc/assert.h>

// largest possible orders
#define OFFSET_ORDER (uint8_t) 253
#define CHUNK_ORDER (uint8_t) 169

//#define OFFSET_ORDER (uint8_t) 3
//#define CHUNK_ORDER (uint8_t) 3


// global caches for both the btrees
STRUCT_BTREE_MAN(offset_cache, 200, OFFSET_ORDER, 1) ;						
STRUCT_BTREE_MAN(chunk_cache, 200, CHUNK_ORDER, 2) ;			   	

struct chunk
{
	uint64_t npages ;
	uint64_t offset ;	
} ;

//////////////////////////////
// freelist resource manager
//////////////////////////////

static int	
frm_node(struct btree *tree, offset_t offset, struct btree_node **store, void *arg)
{
	struct frm *f = (struct frm *) arg ;
	return btree_man_node(tree, offset, store, f->cache) ;
}

static int
frm_rem(void *arg, offset_t offset) 
{
	struct frm *f = arg ;
	
	if (f->n_free > FRM_BUF_SIZE)
		panic("freelist node manager overflow") ;
	
	if (f->n_use < FRM_BUF_SIZE) {
		f->to_use[f->n_use++] = offset ;
	}
	else {
		f->to_free[f->n_free] = offset ;
		f->n_free++ ;
		if (f->n_free > (FRM_BUF_SIZE / 2))
			f->service = 1 ;
	}
	
	return btree_man_rem(f->cache, offset) ;
}

static int
frm_alloc(struct btree *tree, struct btree_node **store, void *arg)
{
	struct frm *f = (struct frm *) arg ;

	if (f->n_use == 0)
		panic("frm_alloc: freelist node manager underflow") ;

	offset_t offset = f->to_use[f->n_use - 1] ;
	
	f->to_use[f->n_use - 1] = 0 ;
	f->n_use-- ;
	if (f->n_use < (FRM_BUF_SIZE / 2))
		f->service = 1 ;
	
	int r = btree_man_alloc(tree, offset, store, f->cache) ;
	
	return r ;
}

static int 
frm_unpin(void *arg)
{
	struct frm *f = (struct frm *) arg ;
	return btree_man_unpin(f->cache) ;
}

static void 
frm_service_one(struct frm *f, struct freelist *l)
{
	// may get set during execution of function
	f->service = 0 ;	

	while (f->n_free) {
		uint64_t to_free = f->to_free[--f->n_free] ;
		// may inc f->n_free, or dec f->n_use
		assert(freelist_free(l, to_free, 1) == 0) ;
	}

	if (f->n_use >= FRM_BUF_SIZE / 2)
		return ;

	// XXX: think about this a little more...
	uint64_t npages = FRM_BUF_SIZE - (f->n_use + 3) ;
	uint64_t base ; 
	
	// may inc f->n_free, or dec f->n_use
	assert((base = freelist_alloc(l, npages)) > 0) ;

	for (int i = 0 ; i < npages ; i++) {
		f->to_use[f->n_use++] = base + i ;
	}
		
	assert(f->n_use <= 10) ;
}

static void
frm_service(struct freelist *l)
{
	struct frm *chunk_manager = &l->chunk_manager ;
	struct frm *offset_manager = &l->offset_manager ;
	
	while (chunk_manager->service && !chunk_manager->servicing  && 
		   !offset_manager->servicing) {
		chunk_manager->servicing = 1 ;
		frm_service_one(chunk_manager, l) ;
		chunk_manager->servicing = 0 ;
	}
	while (offset_manager->service && !offset_manager->servicing && 
		   !chunk_manager->servicing) {
		offset_manager->servicing = 1 ;
		frm_service_one(offset_manager, l) ;
		offset_manager->servicing = 0 ;
	}	
}

static int 
frm_write(struct btree_node *node, void *arg)
{
	struct frm *f = (struct frm *) arg ;
	return btree_man_write(node, f->cache) ;
}

static void
frm_reset(struct frm *f, struct btree_man *cache)
{
	f->manager.alloc = &frm_alloc ;
	f->manager.free = &frm_rem ;
	f->manager.node = &frm_node ;
	f->manager.arg = f ;
	f->manager.unpin = &frm_unpin ;
	f->manager.write = &frm_write ;
	
	f->cache = cache ;

	btree_man_init(f->cache) ;
}

static void
frm_setup(struct frm *f, struct btree_man *cache)
{
	frm_reset(f, cache) ;
}

static int 
frm_init(struct frm *f, struct btree_man *cache, uint64_t base, uint64_t npages)
{
	memset(f, 0, sizeof(struct frm)) ;

	frm_reset(f, cache) ;
	
	for (int i = 0 ; i < npages ; i++)
		f->to_use[i] = base + i ;
	f->n_use = npages ;
	
	return btree_man_init(f->cache) ;
}

//////////////////////////////
// freelist
//////////////////////////////

static int64_t
freelist_insert(struct freelist *l, uint64_t offset, uint64_t npages)
{
	struct chunk k = { npages, offset } ;
	
	btree_insert(&l->chunks, (offset_t *) &k, offset) ;
	btree_insert(&l->offsets, &offset, npages) ;
	
	return 0 ;	
}

int64_t 
freelist_alloc(struct freelist *l, uint64_t npages)
{
	struct chunk k = { npages, 0 } ;
	int64_t offset ;
	
	// XXX: optimize...
	
	int64_t val = btree_gtet(&l->chunks,
						   (uint64_t *)&k,
						   (uint64_t *)&k) ;
	
	if (val < 0)
		return -E_NO_SPACE ;
	
	if (1) {
		btree_delete(&l->offsets, &k.offset) ;
		btree_delete(&l->chunks, (uint64_t *)&k) ;
		
		k.npages -= npages ;
		if (k.npages != 0) {
			btree_insert(&l->offsets, &k.offset, k.npages) ;
			btree_insert(&l->chunks, (uint64_t *)&k, k.offset) ;
		}
		
		offset = k.offset + k.npages ;		
		
		frm_service(l) ;	
	
		l->free -= npages ;
		
		return offset ;
	}
	return -E_NO_SPACE ;
}

int 
freelist_free(struct freelist *l, uint64_t base, uint64_t npages)
{
	offset_t l_base = base - 1 ;
	offset_t g_base = base + 1 ;

	// XXX: could also be optimized...

	l->free += npages ;
	
	int64_t l_npages = btree_ltet(&l->offsets,
						   	  (uint64_t *)&l_base,
						   	  (uint64_t *)&l_base) ;

	int64_t g_npages = btree_gtet(&l->offsets,
						   	  (uint64_t *)&g_base,
						   	  (uint64_t *)&g_base) ;

	
	char l_merge = 0 ;
	char g_merge = 0 ;

	
	if (l_npages > 0) {
		
		if (l_base + l_npages == base)
			l_merge = 1 ;
	}
	if (g_npages > 0) {
		
		if (base + npages == g_base)
			g_merge = 1 ;
	}

	if (l_merge) {
		base = l_base ;
		npages += l_npages ;
		
		btree_delete(&l->offsets, &l_base) ;
		struct chunk k = { l_npages, l_base } ;
		btree_delete(&l->chunks, (uint64_t *) &k) ;
		
	}
	
	if (g_merge) {
		npages += g_npages ;
		
		btree_delete(&l->offsets, &g_base) ;
		struct chunk k = { g_npages, g_base } ;
		btree_delete(&l->chunks, (uint64_t *) &k) ;
	}

	

	btree_insert(&l->offsets, &base, npages) ;
	struct chunk k = { npages, base } ;
	btree_insert(&l->chunks, (uint64_t *) &k, base) ;
	
	frm_service(l) ;
	
	return 0 ;
}

void
freelist_setup(uint8_t *b)
{
	struct freelist *l = (struct freelist *)b ;
	
	frm_setup(&l->chunk_manager, &chunk_cache) ;
	frm_setup(&l->offset_manager, &offset_cache) ;
}

void 
freelist_serialize(struct freelist *f)
{
	f->chunk_manager.manager.arg = &f->chunk_manager ;
	f->offset_manager.manager.arg = &f->offset_manager ;

	f->chunks.mm = &f->chunk_manager.manager ;
	f->offsets.mm = &f->offset_manager.manager ;
	
	return ;	
}

int 
freelist_init(struct freelist *l, uint64_t base, uint64_t npages)
{
	int r ;
	
	static_assert(BTREE_NODE_SIZE(CHUNK_ORDER, 2) <= PGSIZE) ;
	static_assert(BTREE_NODE_SIZE(OFFSET_ORDER, 1) <= PGSIZE) ;
	
	frm_init(&l->chunk_manager, &chunk_cache, base, FRM_BUF_SIZE) ;
	base += FRM_BUF_SIZE ;
	npages -= FRM_BUF_SIZE ;

	frm_init(&l->offset_manager, &offset_cache, base, FRM_BUF_SIZE) ;
	base += FRM_BUF_SIZE ;
	npages -= FRM_BUF_SIZE ;
	
	btree_init(&l->chunks, CHUNK_ORDER, 2, &l->chunk_manager.manager) ;
	btree_init(&l->offsets, OFFSET_ORDER, 1, &l->offset_manager.manager) ;

	if ((r = freelist_insert(l, base, npages)) < 0)
		return r ;
		
	l->free = npages ;
		
	return 0 ;
}

//////////////////////////////
// debug
//////////////////////////////
#include <lib/btree/cache.h>

void
freelist_pretty_print(struct freelist *l)
{
	cprintf("*chunk tree (%ld)*\n",btree_size(&l->chunks)) ;
	bt_pretty_print(&l->chunks, l->chunks.root, 0) ;
	cprintf("*offset tree (%ld)*\n", btree_size(&l->offsets)) ;
	bt_pretty_print(&l->offsets, l->offsets.root, 0) ;
	cprintf("num free %ld\n", l->free) ;
	cprintf("num pinned %d\n", 
			cache_num_pinned(l->chunk_manager.cache->cache)) ;
	cprintf("num pinned %d\n", 
			cache_num_pinned(l->offset_manager.cache->cache)) ;
}
