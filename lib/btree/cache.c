#include <lib/btree/cache.h>
#include <inc/error.h>
#include <inc/string.h>

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
		if (!c->meta[i].inuse) {
			c->meta[i].inuse = 1 ;
			c->meta[i].tag = t ;
			c->meta[i].pin = 1 ;
			*store = &c->buf[i * c->s_ent] ;
			return 1 ;
		}
	}

	*store = 0 ; 
	return -E_NO_SPACE ;	
}

int 
cache_ent(struct cache *c, tag_t t, uint8_t **store) 
{
	int i = 0 ;
	for(; i < c->n_ent ; i++) {
		if (c->meta[i].inuse && t == c->meta[i].tag) {
			*store = &c->buf[i * c->s_ent] ;
			c->meta[i].pin = 1;
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
			memset(&c->meta[i], 0 , sizeof(struct emeta)) ;
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
cache_num_pinned(struct cache *c)
{
	int n = 0 ;

	int i = 0 ;
	for(; i < c->n_ent ; i++)
		if (c->meta[i].inuse && c->meta[i].pin)
			n++ ;

	return n ;
}

int	
cache_init(struct cache *c) 
{
	memset(c->buf, 0, c->s_buf) ;
	memset(c->meta, 0, c->n_ent * sizeof(struct emeta)) ;
	
	return 0 ;
}

