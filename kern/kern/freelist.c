#include <machine/mmu.h>
#include <kern/freelist.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/stdio.h>
#include <inc/assert.h>

#define OFFSET_ORDER 	BTREE_MAX_ORDER1
#define CHUNK_ORDER 	BTREE_MAX_ORDER2

// global caches for both the btrees
//STRUCT_BTREE_MAN(offset_cache, 200, OFFSET_ORDER, 1) ;						
STRUCT_BTREE_CACHE(offset_cache, 200, OFFSET_ORDER, 1) ;						
//STRUCT_BTREE_MAN(chunk_cache, 200, CHUNK_ORDER, 2) ;		
STRUCT_BTREE_CACHE(chunk_cache, 200, CHUNK_ORDER, 2) ;				   	

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
	return btree_simple_node(tree, offset, store, &f->simple) ;
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
	
	return btree_simple_rem(&f->simple, offset) ;
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
	
	int r = btree_simple_alloc(tree, offset, store, &f->simple) ;
	
	return r ;
}

static int 
frm_unpin(void *arg)
{
	struct frm *f = (struct frm *) arg ;
	return btree_simple_unpin(&f->simple) ;
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
	struct frm *chunk_manager = &l->chunk_frm ;
	struct frm *offset_manager = &l->offset_frm ;
	
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
	return btree_simple_write(node, &f->simple) ;
}

static void
frm_reset(struct frm *f, uint8_t order, struct cache *cache, struct btree_manager *manager)
{
	btree_simple_init(&f->simple, order, cache) ;

	manager->alloc = &frm_alloc ;
	manager->free = &frm_rem ;
	manager->node = &frm_node ;
	manager->arg = f ;
	manager->unpin = &frm_unpin ;
	manager->write = &frm_write ;
}

static void
frm_setup(struct frm *f, uint8_t order, struct cache *cache, struct btree_manager *manager)
{
	frm_reset(f, order, cache, manager) ;
}

static int 
frm_init(struct frm *f, uint64_t base, uint64_t npages, uint8_t order, struct cache *cache, struct btree_manager *manager)
{
	memset(f, 0, sizeof(struct frm)) ;

	frm_reset(f, order, cache, manager) ;
	
	for (int i = 0 ; i < npages ; i++)
		f->to_use[i] = base + i ;
	f->n_use = npages ;
	
	return 0 ;
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
	
	frm_setup(&l->chunk_frm, CHUNK_ORDER, &chunk_cache, &l->chunks.manager) ;
	frm_setup(&l->offset_frm, OFFSET_ORDER, &offset_cache, &l->offsets.manager) ;
}

void 
freelist_serialize(struct freelist *f)
{
	/*
	f->chunk_manager.manager.arg = &f->chunk_manager ;
	f->offset_manager.manager.arg = &f->offset_manager ;

	f->chunks.mm = &f->chunk_manager.manager ;
	f->offsets.mm = &f->offset_manager.manager ;
	
	return ;	
	*/
}

int 
freelist_init(struct freelist *l, uint64_t base, uint64_t npages)
{
	int r ;
	
	static_assert(BTREE_NODE_SIZE(CHUNK_ORDER, 2) <= PGSIZE) ;
	static_assert(BTREE_NODE_SIZE(OFFSET_ORDER, 1) <= PGSIZE) ;
	
	// XXX: make frm like btree_default
	
	struct btree_manager temp1 ;
	struct btree_manager temp2 ;
	
	frm_init(&l->chunk_frm, base, FRM_BUF_SIZE, CHUNK_ORDER, &chunk_cache, &temp1) ;
	base += FRM_BUF_SIZE ;
	npages -= FRM_BUF_SIZE ;

	frm_init(&l->offset_frm, base, FRM_BUF_SIZE, OFFSET_ORDER, &offset_cache, &temp2) ;
	base += FRM_BUF_SIZE ;
	npages -= FRM_BUF_SIZE ;
	
	btree_init(&l->chunks, CHUNK_ORDER, 2, &temp1) ;
	btree_init(&l->offsets, OFFSET_ORDER, 1, &temp2) ;

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
	btree_pretty_print(&l->chunks, l->chunks.root, 0) ;
	cprintf("*offset tree (%ld)*\n", btree_size(&l->offsets)) ;
	btree_pretty_print(&l->offsets, l->offsets.root, 0) ;
	cprintf("num free %ld\n", l->free) ;
	cprintf("num pinned %d\n", 
			cache_num_pinned(l->chunk_frm.simple.cache)) ;
	cprintf("num pinned %d\n", 
			cache_num_pinned(l->offset_frm.simple.cache)) ;
}
