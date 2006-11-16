#include <btree/pbtree.h>
#include <btree/pbtree_frm.h>
#include <btree/btree_manager.h>
#include <btree/cache.h>
#include <btree/btree.h>
#include <kern/freelist.h>
#include <kern/log.h>
#include <kern/arch.h>
#include <inc/error.h>

int
frm_free(uint64_t id, offset_t offset, void *arg)
{
    struct frm *f = arg;

    if (f->n_free > FRM_BUF_SIZE)
	panic("freelist node manager overflow");

    if (f->n_use < FRM_BUF_SIZE) {
	f->to_use[f->n_use++] = offset;
    } else {
	f->to_free[f->n_free] = offset;
	f->n_free++;
	if (f->n_free > (FRM_BUF_SIZE / 2))
	    f->service = 1;
    }

    log_free(offset);
    return cache_rem(btree_cache(id), offset);
}

int
frm_new(uint64_t id, uint8_t ** mem, uint64_t * off, void *arg)
{
    struct frm *f = (struct frm *) arg;

    if (f->n_use == 0)
	panic("frm_alloc: freelist node manager underflow");

    offset_t offset = f->to_use[f->n_use - 1];

    f->to_use[f->n_use - 1] = 0;
    f->n_use--;
    if (f->n_use < (FRM_BUF_SIZE / 2))
	f->service = 1;

    uint8_t *buf;
    if ((cache_alloc(btree_cache(id), offset, &buf)) < 0) {
	cprintf("new: cache fully pinned (%d)\n", btree_cache(id)->n_ent);
	*mem = 0;
	*off = 0;
	return -E_NO_SPACE;
    }

    *mem = buf;
    *off = offset;

    return 0;
}

void
frm_service_one(struct frm *f, struct freelist *l)
{
    // may get set during execution of function
    f->service = 0;

    while (f->n_free) {
	uint64_t to_free = f->to_free[--f->n_free];
	// may inc f->n_free, or dec f->n_use
	freelist_free_later(l, to_free, BTREE_BLOCK_SIZE);
    }

    if (f->n_use >= FRM_BUF_SIZE / 2)
	return;

    // XXX: think about this a little more...
    uint64_t nblocks = FRM_BUF_SIZE - (f->n_use + 3);
    uint64_t base;

    for (uint32_t i = 0; i < nblocks; i++) {
	assert((base = freelist_alloc(l, BTREE_BLOCK_SIZE)) > 0);
	f->to_use[f->n_use++] = base;
    }

    assert(f->n_use <= FRM_BUF_SIZE);
}

void
frm_service(struct freelist *l)
{
    struct frm *chunk_manager = &l->chunk_frm;
    struct frm *offset_manager = &l->offset_frm;

    while (chunk_manager->service && !chunk_manager->servicing
	   && !offset_manager->servicing) {
	chunk_manager->servicing = 1;
	frm_service_one(chunk_manager, l);
	chunk_manager->servicing = 0;
    }
    while (offset_manager->service && !offset_manager->servicing
	   && !chunk_manager->servicing) {
	offset_manager->servicing = 1;
	frm_service_one(offset_manager, l);
	offset_manager->servicing = 0;
    }
}

uint32_t
frm_init(struct frm *f, uint64_t base)
{
    memset(f, 0, sizeof(struct frm));

    f->n_use = FRM_BUF_SIZE;
    for (uint32_t i = 0; i < f->n_use; i++)
	f->to_use[i] = base + i * BTREE_BLOCK_SIZE;

    return f->n_use * BTREE_BLOCK_SIZE;
}
