#include <lib/btree/btree.h>
#include <lib/btree/cache.h>
#include <kern/lib.h>
#include <inc/error.h>

int
cache_num_ent(struct cache *c)
{
	int num = 0 ;

	int i = 0 ;
	for(; i < c->n_ent ; i++)
		if (c->meta[i].inuse) 
			num++ ;
			
	return num ;
}

int 
cache_alloc(struct cache *c, tag_t tag, uint8_t **store) 
{
	if (tag == 0)
		return -E_INVAL ;
	
	struct cmeta *cm = TAILQ_FIRST(&c->lru_stack) ;
	if (cm->inuse) {
		while (cm->ref) {
			cm = TAILQ_NEXT(cm, cm_link) ;
			if (cm == NULL) {
				*store = 0 ;
				return -E_NO_SPACE ;
			}
		}
		
		if (!cm->inuse)
			cprintf("cache_alloc: lru eviction error") ;
		memset(&c->buf[cm->index * c->s_ent], 0 , c->s_ent) ;
	}
	
	TAILQ_REMOVE(&c->lru_stack, cm, cm_link) ;
	TAILQ_INSERT_TAIL(&c->lru_stack, cm, cm_link) ;
	
	cm->inuse = 1 ;
	cm->ref = 1 ;
	cm->tag = tag ;
	*store = &c->buf[cm->index * c->s_ent] ;
	
	return 0 ;
}

int
cache_try_insert(struct cache *c, tag_t t, uint8_t *src, uint8_t **store)
{
	if (t == 0)
		return -E_INVAL ;
	
	int r ; 
	r = cache_ent(c, t, store) ;
	if (r != -E_NOT_FOUND)
		return r ;	// may be zero

	r = cache_alloc(c, t, store) ;
	if (r < 0)
		return r ;

	memcpy(*store, src, c->s_ent) ;

	return 0 ;
}

int 
cache_ent(struct cache *c, tag_t t, uint8_t **store) 
{
	if (t == 0)
		return -E_INVAL ;
	
	int i = 0 ;
	for(; i < c->n_ent ; i++) {
		if (c->meta[i].inuse && t == c->meta[i].tag) {
			*store = &c->buf[i * c->s_ent] ;
			struct cmeta *cm  = &c->meta[i] ;			
			TAILQ_REMOVE(&c->lru_stack, cm, cm_link);
			cm->ref++ ;
			TAILQ_INSERT_TAIL(&c->lru_stack, cm, cm_link);
			return 0 ;
		}
	}
	
	*store = 0 ; 
	return -E_NOT_FOUND ;	
}

int 
cache_rem(struct cache *c, tag_t t)
{
	if (t == 0)
		return -E_INVAL ;
	
	int i = 0 ;
	for(; i < c->n_ent ; i++) {
		if (c->meta[i].inuse && t == c->meta[i].tag) {
			memset(&c->buf[i * c->s_ent], 0, c->s_ent) ;
			//memset(&c->meta[i], 0 , sizeof(struct cmeta)) ;
			c->meta[i].inuse = 0 ;
			c->meta[i].ref--;
			
			if (c->meta[i].ref != 0)
                panic("cache_rem: entry %ld still have refs: %d", t, c->meta[i].ref) ;
			
			c->meta[i].tag = 0 ;
			TAILQ_REMOVE(&c->lru_stack, &c->meta[i], cm_link);
			TAILQ_INSERT_HEAD(&c->lru_stack, &c->meta[i], cm_link);
			return 1 ;
		}
	}
	return -E_NOT_FOUND ;		
}

int
cache_inc_ref(struct cache *c, tag_t t)
{
	if (t == 0)
		return -E_INVAL ;
	
	int i = 0 ;
	for(; i < c->n_ent ; i++) {
		if (c->meta[i].inuse && t == c->meta[i].tag) {
			c->meta[i].ref++ ;
			return 0 ;
		}
	}
	
	return -E_NOT_FOUND ;		
}

int 
cache_dec_ref(struct cache *c, tag_t t)
{
	if (t == 0)
		return -E_INVAL ;
	
	int i = 0 ;
	for(; i < c->n_ent ; i++) {
		if (c->meta[i].inuse && t == c->meta[i].tag) {
			c->meta[i].ref-- ;
			return 0 ;
		}
	}
	
	return -E_NOT_FOUND ;		
}

int 
cache_num_pinned(struct cache *c)
{
	int n = 0 ;

	int i = 0 ;
	for(; i < c->n_ent ; i++)
		if (c->meta[i].inuse && c->meta[i].ref)
			n++ ;

	return n ;
}

static void
cache_print(struct cache *c)
{
	for (int i = 0 ; i < c->n_ent ; i++)
		if (c->meta[i].inuse)
			cprintf("c->meta[i].tag %ld\n", c->meta[i].tag) ;
}

static char __attribute__((unused))
cache_sanity_check(struct cache *c)
{
	for (int i = 0 ; i < c->n_ent ; i++) {
		if (!c->meta[i].inuse)
			continue ;
		tag_t t = c->meta[i].tag ;
		for (int j = i + 1 ; j < c->n_ent ; j++) {	
			if (!c->meta[j].inuse)
				continue ;
			if (c->meta[j].tag == t) {
				cache_print(c) ;
				return -1 ;	
			}
		}
	}
	return 0 ;
}

int	
cache_init(struct cache *c)
{
	memset(c->buf, 0, c->s_buf) ;
	memset(c->meta, 0, c->n_ent * sizeof(struct cmeta)) ;
	
	TAILQ_INIT(&c->lru_stack) ;
	for (int i = 0 ; i < c->n_ent ; i++) {
		c->meta[i].index = i ;
		TAILQ_INSERT_HEAD(&c->lru_stack, &c->meta[i], cm_link);
	}

	return 0 ;
}

