#ifndef DSTACK_H_
#define DSTACK_H_

#include <inc/queue.h>
#include <inc/types.h>

// limited dynamic stack impl

struct dstack {
    int sp;
    LIST_HEAD(top, dstack_page) pages;
};

void dstack_init(struct dstack *s);
int  dstack_push(struct dstack *s, uint64_t n)
    __attribute__((warn_unused_result));
uint64_t dstack_pop(struct dstack *s)
    __attribute__((warn_unused_result));
char dstack_empty(struct dstack *s)
    __attribute__((warn_unused_result));

#endif /*DSTACK_H_ */
