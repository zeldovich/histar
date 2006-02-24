#include <lib/btree/btree_debug.h>
#include <machine/memlayout.h>
#include <kern/freelist.h>
#include <kern/lib.h>
#include <inc/error.h>

#define OFFSET_ORDER 	BTREE_MAX_ORDER1
#define CHUNK_ORDER 	BTREE_MAX_ORDER2


// global caches for both the btrees
STRUCT_BTREE_CACHE(offset_cache, 200, OFFSET_ORDER, 1) ;
STRUCT_BTREE_CACHE(chunk_cache, 200, CHUNK_ORDER, 2) ;

struct chunk
{
	uint64_t nbytes ;
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
frm_unpin_node(void *arg, offset_t off)
{
	struct frm *f = (struct frm *) arg ;
	return btree_simple_unpin_node(&f->simple, off) ;
}


static void 
frm_service_one(struct frm *f, struct freelist *l)
{
	// may get set during execution of function
	f->service = 0 ;	

	while (f->n_free) {
		uint64_t to_free = f->to_free[--f->n_free] ;
		// may inc f->n_free, or dec f->n_use
		assert(freelist_free(l, to_free, BTREE_BLOCK_SIZE) == 0) ;
	}

	if (f->n_use >= FRM_BUF_SIZE / 2)
		return ;

	// XXX: think about this a little more...
	uint64_t nblocks = FRM_BUF_SIZE - (f->n_use + 3) ;
	uint64_t base ; 
	
	// may inc f->n_free, or dec f->n_use
	assert((base = freelist_alloc(l, nblocks * BTREE_BLOCK_SIZE)) > 0) ;

	for (uint32_t i = 0 ; i < nblocks ; i++) {
		f->to_use[f->n_use++] = base + i * BTREE_BLOCK_SIZE ;
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
frm_reset(struct frm *f, uint8_t order, struct cache *cache)
{
	btree_simple_reset(&f->simple, order, cache) ;

	struct btree_manager mm ;
	mm.alloc = &frm_alloc ;
	mm.free = &frm_rem ;
	mm.node = &frm_node ;
	mm.arg = f ;
	mm.unpin_node = &frm_unpin_node ;
	mm.write = &frm_write ;
	
	btree_manager_is(&f->simple.tree, &mm) ;
}

static void
frm_deserialize(struct frm *f, uint8_t order, struct cache *cache, void *buf)
{
	memcpy(f, buf, sizeof(*f)) ;
	frm_reset(f, order, cache)	 ;
}

static uint32_t
frm_init(struct frm *f, uint64_t base, 
		uint8_t order, uint8_t key_size, 
		uint8_t value_size, struct cache *cache)
{
	memset(f, 0, sizeof(struct frm)) ;
	btree_init(&f->simple.tree, order, key_size, value_size, NULL) ;
	frm_reset(f, order, cache) ;
	
	f->n_use = FRM_BUF_SIZE ;
	for (uint32_t i = 0 ; i < f->n_use ; i++)
		f->to_use[i] = base + i * BTREE_BLOCK_SIZE ;
	
	return f->n_use * BTREE_BLOCK_SIZE ;
}

//////////////////////////////
// freelist
//////////////////////////////

static int64_t
freelist_insert(struct freelist *l, uint64_t offset, uint64_t nbytes)
{
	struct chunk k = { nbytes, offset } ;
	
	btree_insert(&l->chunk_frm, (offset_t *) &k, &offset) ;
	btree_insert(&l->offset_frm, &offset, &nbytes) ;
	
	return 0 ;	
}

int64_t 
freelist_alloc(struct freelist *l, uint64_t nbytes)
{
	struct chunk k = { nbytes, 0 } ;
	int64_t offset ;
	
	// XXX: optimize...
	
	int64_t val ;
	int r = btree_gtet(&l->chunk_frm,
						   (uint64_t *)&k,
						   (uint64_t *)&k,
						   &val) ;
	
	
	if (r < 0)
		return -E_NO_SPACE ;
	
	btree_delete(&l->offset_frm, &k.offset) ;
	btree_delete(&l->chunk_frm, (uint64_t *)&k) ;
	
	k.nbytes -= nbytes ;
	if (k.nbytes != 0) {
		btree_insert(&l->offset_frm, &k.offset, &k.nbytes) ;
		btree_insert(&l->chunk_frm, (uint64_t *)&k, &k.offset) ;
	}
	
	offset = k.offset + k.nbytes ;		
	
	frm_service(l) ;	

	l->free -= nbytes ;
	
	return offset ;

}

int 
freelist_free(struct freelist *l, uint64_t base, uint64_t nbytes)
{
	offset_t l_base = base - 1 ;
	offset_t g_base = base + 1 ;

	// XXX: could also be optimized...

	l->free += nbytes ;
	
	int64_t l_nbytes ; 
	int rl = btree_ltet(&l->offset_frm,
						(uint64_t *)&l_base,
						(uint64_t *)&l_base,
						&l_nbytes) ;

	int64_t g_nbytes ;  
	int rg = btree_gtet(&l->offset_frm,
			   (uint64_t *)&g_base,
		   	   (uint64_t *)&g_base,
		   	   &g_nbytes) ;

	
	char l_merge = 0 ;
	char g_merge = 0 ;

	
	if (rl == 0) {
		
		if (l_base + l_nbytes == base)
			l_merge = 1 ;
	}
	if (rg == 0) {
		
		if (base + nbytes == g_base)
			g_merge = 1 ;
	}

	if (l_merge) {
		base = l_base ;
		nbytes += l_nbytes ;
		
		btree_delete(&l->offset_frm, &l_base) ;
		struct chunk k = { l_nbytes, l_base } ;
		btree_delete(&l->chunk_frm, (uint64_t *) &k) ;
		
	}
	
	if (g_merge) {
		nbytes += g_nbytes ;
		
		btree_delete(&l->offset_frm, &g_base) ;
		struct chunk k = { g_nbytes, g_base } ;
		btree_delete(&l->chunk_frm, (uint64_t *) &k) ;
	}

	

	btree_insert(&l->offset_frm, &base, &nbytes) ;
	struct chunk k = { nbytes, base } ;
	btree_insert(&l->chunk_frm, (uint64_t *) &k, &base) ;
	
	frm_service(l) ;
	
	return 0 ;
}

static struct 
{
#define FREE_LATER_SIZE PGSIZE
	uint64_t off[FREE_LATER_SIZE] ;
	uint64_t nbytes[FREE_LATER_SIZE] ;
	
	int size ;
} free_later ;

void
freelist_free_later(struct freelist *l, uint64_t base, uint64_t nbytes)
{
	int i = free_later.size ;
	if (i >= FREE_LATER_SIZE)
		panic("freelist_free_later: need to implement...") ;
	free_later.off[i] = base ;
	free_later.nbytes[i] = nbytes ;
	
	free_later.size++ ;
}

int
freelist_commit(struct freelist *l)
{
	int n = free_later.size ;
	for (int i = 0 ; i < n ; i++) {
		uint64_t base = free_later.off[i] ;
		uint64_t nbytes = free_later.nbytes[i] ;
		int r = freelist_free(l, base, nbytes) ;
		if (r < 0)
			return r;
	}
	free_later.size = 0 ;
	return 0;
}

void
freelist_deserialize(struct freelist *l, void *buf)
{
	struct freelist *l2 = (struct freelist *) buf ;
	frm_deserialize(&l->chunk_frm, CHUNK_ORDER, &chunk_cache, &l2->chunk_frm) ;
	frm_deserialize(&l->offset_frm, OFFSET_ORDER, &offset_cache, &l2->offset_frm) ;
	l->free = l2->free ;

	// XXX
	free_later.size = 0 ;
}
void
freelist_serialize(void *buf, struct freelist *l)
{
	memcpy(buf, l, sizeof(*l)) ;	
}

int 
freelist_init(struct freelist *l, uint64_t base, uint64_t nbytes)
{
	int r ;
	
	static_assert(BTREE_NODE_SIZE(CHUNK_ORDER, 2) <= BTREE_BLOCK_SIZE) ;
	static_assert(BTREE_NODE_SIZE(OFFSET_ORDER, 1) <= BTREE_BLOCK_SIZE) ;
	
	uint32_t frm_bytes;
	frm_bytes = frm_init(&l->chunk_frm, base, CHUNK_ORDER, 2, 1, &chunk_cache) ;
	base += frm_bytes;
	nbytes -= frm_bytes;

	frm_bytes = frm_init(&l->offset_frm, base, OFFSET_ORDER, 1, 1, &offset_cache) ;
	base += frm_bytes;
	nbytes -= frm_bytes;
	
	if ((r = freelist_insert(l, base, nbytes)) < 0)
		return r ;
		
	l->free = nbytes ;
		
	free_later.size = 0 ;
		
	return 0 ;
}

//////////////////////////////
// debug
//////////////////////////////

#include <lib/btree/cache.h>

void
freelist_pretty_print(struct freelist *l)
{
	cprintf("*chunk tree (%ld)*\n",btree_size(&l->chunk_frm.simple.tree)) ;
	btree_pretty_print(&l->chunk_frm.simple.tree) ;
	cprintf("*offset tree (%ld)*\n", btree_size(&l->offset_frm.simple.tree)) ;
	btree_pretty_print(&l->offset_frm.simple.tree) ;
	cprintf("num free %ld\n", l->free) ;
	cprintf("num pinned %d\n", 
			cache_num_pinned(l->chunk_frm.simple.cache)) ;
	cprintf("num pinned %d\n", 
			cache_num_pinned(l->offset_frm.simple.cache)) ;
}

