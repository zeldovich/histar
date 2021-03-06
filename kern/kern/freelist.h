#ifndef FREELIST_H_
#define FREELIST_H_

#include <btree/btree.h>
#include <btree/pbtree_frm.h>

struct freelist {
    struct frm frm;
};

void	freelist_init(struct freelist *l, uint64_t base, uint64_t nbytes);
void	freelist_setup(uint8_t *b);
int64_t freelist_alloc(struct freelist *l, uint64_t nbytes);
void	freelist_free_later(struct freelist *l, uint64_t base, uint64_t nbytes);
int 	freelist_commit(struct freelist *l)
    __attribute__ ((warn_unused_result));

void	freelist_deserialize(struct freelist *l, void *buf);
void	freelist_serialize(void *buf, struct freelist *l);

// debug
void freelist_pretty_print(struct freelist *l);
void freelist_sanity_check(struct freelist *l);

#endif /*FREELIST_H_*/
