#ifndef CACHE_H_
#define CACHE_H_

#include <inc/types.h>
#include <inc/queue.h>

typedef uint64_t tag_t ;

// use to declare a struct cache
#define STRUCT_CACHE(name, n_ent, s_ent)	\
	uint8_t name##_buf[(n_ent) * (s_ent)] ;	\
	struct cmeta name##_cmeta[(n_ent * sizeof(struct cmeta))] ;	\
	struct cache name =	{(n_ent), (s_ent), ((int)(n_ent) * (int)(s_ent)), name##_buf, name##_cmeta} ;


struct cmeta
{
	uint8_t inuse ;
	uint8_t	pin ;
	
	tag_t		tag ;
	uint16_t	index ;

    TAILQ_ENTRY(cmeta) cm_link;
} ;

TAILQ_HEAD(cmeta_list, cmeta);

struct cache
{
	uint16_t n_ent ;
	uint16_t s_ent ;
	uint32_t s_buf ;

	// parallel
	uint8_t			*buf ;
	struct cmeta 	*meta ;
	
	struct cmeta_list lru_stack ;
} ;

int	cache_init(struct cache *c) ;
int cache_alloc(struct cache *c, tag_t t, uint8_t **store) ;
int cache_ent(struct cache *c, tag_t t, uint8_t **store) ;
int cache_rem(struct cache *c, tag_t t) ;
int cache_unpin(struct cache *c) ;

int cache_num_ent(struct cache *c) ;
int cache_num_pinned(struct cache *c) ;


#endif /*CACHE_H_*/
