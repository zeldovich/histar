#ifndef CACHE_H_
#define CACHE_H_

#include <inc/types.h>

typedef uint64_t tag_t ;

// use to declare a struct cache
#define STRUCT_CACHE(name, n_ent, s_ent)	\
	uint8_t name##_buf[(n_ent) * (s_ent)] ;	\
	struct emeta name##_emeta[(n_ent * sizeof(struct emeta))] ;	\
	struct cache name =	{(n_ent), (s_ent), ((int)(n_ent) * (int)(s_ent)), name##_buf, name##_emeta} ;


struct emeta
{
	uint8_t inuse ;
	uint8_t	pin ;
	
	tag_t	tag ;
} ;

struct cache
{
	uint16_t n_ent ;
	uint16_t s_ent ;
	uint32_t s_buf ;

	// parallel
	uint8_t			*buf ;
	struct emeta 	*meta ;
	
} ;

int	cache_init(struct cache *c) ;
int cache_alloc(struct cache *c, tag_t t, uint8_t **store) ;
int cache_ent(struct cache *c, tag_t t, uint8_t **store) ;
int cache_rem(struct cache *c, tag_t t) ;
int cache_pin_is(struct cache *c, tag_t t, uint8_t pin) ;

int cache_num_ent(struct cache *c) ;
int cache_num_pinned(struct cache *c) ;

#endif /*CACHE_H_*/
