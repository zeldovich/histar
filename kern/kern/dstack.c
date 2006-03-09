#include <machine/pmap.h>
#include <kern/dstack.h>

#define DSTACK_BUF_SIZE ((PGSIZE - sizeof(LIST_ENTRY(dstack_page))) \
                        / sizeof(uint64_t))

struct dstack_page
{
        LIST_ENTRY(dstack_page) page_link ;       
        uint64_t buf[DSTACK_BUF_SIZE] ;
} ;

void
dstack_init(struct dstack *s)
{
        s->sp = 0 ;
        LIST_INIT(&s->pages) ;
}

int
dstack_push(struct dstack *s, uint64_t n)
{
        int r ;
        
        struct dstack_page *p = LIST_FIRST(&s->pages);
        
        if (s->sp == 0) {
                if ((r = page_alloc((void**)&p)) < 0)
                        return r ;
                LIST_INSERT_HEAD(&s->pages, p, page_link) ;
                s->sp = DSTACK_BUF_SIZE ;       
        }
        
        s->sp-- ;
        p->buf[s->sp] = n ;

        return 0 ;
}

uint64_t
dstack_pop(struct dstack *s)
{
        struct dstack_page *p = LIST_FIRST(&s->pages);
        
        if (s->sp == 0 && p == 0)
                panic("dstack_pop: empty stack") ;
        
        uint64_t ret = p->buf[s->sp] ;
        s->sp++ ;
        
        if (s->sp == DSTACK_BUF_SIZE) {
                LIST_REMOVE(p, page_link) ;
                page_free(p) ;
                s->sp = 0 ;
        }

        return ret ;       
}

char
dstack_empty(struct dstack *s)
{
        return LIST_EMPTY(&s->pages) ;
}
