#include <lib/btree/cache.h>
#include <inc/error.h>
#include <inc/string.h>
#include <lib/btree/btree.h>

#include <inc/stdio.h>
#include <inc/assert.h>

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
cache_alloc(struct cache *c, tag_t t, uint8_t **store) 
{
	int i = 0 ;
	for(; i < c->n_ent ; i++) {
		if (!c->meta[i].inuse) {// || !c->meta[i].pin) {
			if (c->meta[i].inuse) {
				 struct btree_node *node = (struct btree_node *)&c->buf[i * c->s_ent] ;
				cprintf("evicting %ld for %ld\n", node->block.offset, t) ;
				memset(&c->buf[i * c->s_ent], 0 , c->s_ent) ;
				
			}
			c->meta[i].inuse = 1 ;
			c->meta[i].tag = t ;
			c->meta[i].pin = 1 ;
			*store = &c->buf[i * c->s_ent] ;
			
			//assert(cache_sanity_check(c) == 0) ;
			
			//struct cmeta *cm = TAILQ_FIRST(&c->lru_stack) ;
			//cprintf("cm->index %d\n", cm->index) ;
			//TAILQ_REMOVE(&c->lru_stack, cm, cm_link) ;
			//TAILQ_INSERT_TAIL(&c->lru_stack, cm, cm_link) ;
			
			//cprintf("i %d\n", i) ;
			
			return 1 ;
		}
	}
	
	*store = 0 ; 
	return -E_NO_SPACE ;
	/*
	struct cmeta *cm = TAILQ_FIRST(&c->lru_stack) ;
	if (cm->inuse) {
		if (c->on_evict) {
			while (cm->pin) {
				cm = TAILQ_NEXT(cm, cm_link) ;
				if (cm == NULL)
					return -E_NO_SPACE ;
			}
			
			if (!cm->inuse)
				panic("fucker\n") ;
			
		}
		else {
			*store = 0 ; 
			return -E_NO_SPACE ;	
		}
	}
	TAILQ_REMOVE(&c->lru_stack, cm, cm_link) ;
	TAILQ_INSERT_TAIL(&c->lru_stack, cm, cm_link) ;
	
	cm->inuse = 1 ;
	cm->tag = t ;
	cm->pin = 1 ;
	*store = &c->buf[cm->index * c->s_ent] ;
	
	return 0 ;*/
}

int 
cache_ent(struct cache *c, tag_t t, uint8_t **store) 
{
	int i = 0 ;
	for(; i < c->n_ent ; i++) {
		if (c->meta[i].inuse && t == c->meta[i].tag) {
			*store = &c->buf[i * c->s_ent] ;
			struct cmeta *cm  = &c->meta[i] ;			
			//TAILQ_REMOVE(&c->lru_stack, cm, cm_link);
			cm->pin = 1;
			//TAILQ_INSERT_TAIL(&c->lru_stack, cm, cm_link);
			
			return 0 ;
		}
	}
	
	*store = 0 ; 
	return -E_NOT_FOUND ;	
}

int 
cache_rem(struct cache *c, tag_t t)
{
	int i = 0 ;
	for(; i < c->n_ent ; i++) {
		if (c->meta[i].inuse && t == c->meta[i].tag) {
			memset(&c->buf[i * c->s_ent], 0, c->s_ent) ;
			//memset(&c->meta[i], 0 , sizeof(struct cmeta)) ;
			c->meta[i].inuse = 0 ;
			c->meta[i].pin = 0 ;
			c->meta[i].tag = 0 ;
			//TAILQ_REMOVE(&c->lru_stack, &c->meta[i], cm_link);
			//TAILQ_INSERT_HEAD(&c->lru_stack, &c->meta[i], cm_link);
			return 1 ;
		}
	}
	return -E_NOT_FOUND ;		
}

int 
cache_pin_is(struct cache *c, tag_t t, uint8_t pin)
{
	int i = 0 ;
	for(; i < c->n_ent ; i++) {
		if (c->meta[i].inuse && t == c->meta[i].tag) {
			c->meta[i].pin = pin ;
			return 1 ;
		}
	}
	
	return -E_NOT_FOUND ;		
}

int 
cache_unpin(struct cache *c)
{
	int i = 0 ;
	for(; i < c->n_ent ; i++)
		c->meta[i].pin = 0 ;
	return 0 ;	
}

int 
cache_num_pinned(struct cache *c)
{
	int n = 0 ;

	int i = 0 ;
	for(; i < c->n_ent ; i++)
		if (c->meta[i].inuse && c->meta[i].pin)
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
	

/*
	TAILQ_INIT(&c->lru_stack) ;
	for (int i = 0 ; i < c->n_ent ; i++) {
		c->meta[i].index = i ;
		TAILQ_INSERT_HEAD(&c->lru_stack, &c->meta[i], cm_link);
	}
	*/
	/*
	struct cmeta *cm = TAILQ_FIRST(&c->lru_stack) ;
	cprintf("cm->index %d\n", cm->index) ;
	TAILQ_REMOVE(&c->lru_stack, cm, cm_link) ;
	TAILQ_INSERT_TAIL(&c->lru_stack, cm, cm_link) ;
	cm = TAILQ_FIRST(&c->lru_stack) ;
	cprintf("cm->index %d\n", cm->index) ;*/
	return 0 ;
}

