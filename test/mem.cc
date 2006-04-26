#include <test/josenv.hh>
#include <stdio.h>
#include <stdlib.h>

extern "C" {
#include <machine/pmap.h>
}

int
page_alloc(void **pp)
{
    void *p = malloc(PGSIZE);
    assert(p);
    *pp = p;
    return 0;
}

void
page_free(void *p)
{
    free(p);
}
