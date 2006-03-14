#include <kern/freelist.h>
#include <kern/lib.h>
#include <kern/dstack.h>
#include <kern/btree.h>
#include <inc/error.h>

struct chunk {
    uint64_t nbytes;
    uint64_t offset;
};

static int64_t
freelist_insert(struct freelist *l, uint64_t offset, uint64_t nbytes)
{
    struct chunk k = { nbytes, offset };

    int r = btree_insert(BTREE_FCHUNK, (offset_t *) & k, &offset);
    if (r < 0)
	return r;

    r = btree_insert(BTREE_FOFFSET, &offset, &nbytes);
    if (r < 0)
	return r;

    return 0;
}

int64_t
freelist_alloc(struct freelist * l, uint64_t nbytes)
{
    struct chunk k = { nbytes, 0 };
    int64_t offset;

    // XXX: optimize...

    uint64_t val;
    int r = btree_gtet(BTREE_FCHUNK,
		       (uint64_t *) &k,
		       (uint64_t *) &k,
		       &val);
    if (r < 0)
	return -E_NO_SPACE;

    r = btree_delete(BTREE_FOFFSET, &k.offset);
    if (r < 0)
	return r;

    r = btree_delete(BTREE_FCHUNK, (uint64_t *) &k);
    if (r < 0)
	return r;

    k.nbytes -= nbytes;
    if (k.nbytes != 0) {
	r = btree_insert(BTREE_FOFFSET, &k.offset, &k.nbytes);
	if (r < 0)
	    return r;

	r = btree_insert(BTREE_FCHUNK, (uint64_t *) & k, &k.offset);
	if (r < 0)
	    return r;
    }

    offset = k.offset + k.nbytes;

    frm_service(l);

    l->free -= nbytes;

    return offset;

}

int
freelist_free(struct freelist *l, uint64_t base, uint64_t nbytes)
{
    int r;
    offset_t l_base = base - 1;
    offset_t g_base = base + 1;

    // XXX: could also be optimized...

    l->free += nbytes;

    uint64_t l_nbytes;
    int rl = btree_ltet(BTREE_FOFFSET,
			(uint64_t *) & l_base,
			(uint64_t *) & l_base,
			&l_nbytes);

    uint64_t g_nbytes;
    int rg = btree_gtet(BTREE_FOFFSET,
			(uint64_t *) & g_base,
			(uint64_t *) & g_base,
			&g_nbytes);


    char l_merge = 0;
    char g_merge = 0;


    if (rl == 0)
	if (l_base + l_nbytes == base)
	    l_merge = 1;

    if (rg == 0)
	if (base + nbytes == g_base)
	    g_merge = 1;

    if (l_merge) {
	base = l_base;
	nbytes += l_nbytes;

	r = btree_delete(BTREE_FOFFSET, &l_base);
	if (r < 0)
	    return r;

	struct chunk k = { l_nbytes, l_base };
	r = btree_delete(BTREE_FCHUNK, (uint64_t *) & k);
	if (r < 0)
	    return r;
    }

    if (g_merge) {
	nbytes += g_nbytes;

	r = btree_delete(BTREE_FOFFSET, &g_base);
	if (r < 0)
	    return r;

	struct chunk k = { g_nbytes, g_base };
	r = btree_delete(BTREE_FCHUNK, (uint64_t *) & k);
	if (r < 0)
	    return r;
    }

    r = btree_insert(BTREE_FOFFSET, &base, &nbytes);
    if (r < 0)
	return r;

    struct chunk k = { nbytes, base };
    r = btree_insert(BTREE_FCHUNK, (uint64_t *) & k, &base);
    if (r < 0)
	return r;

    frm_service(l);

    return 0;
}

static struct dstack free_later;

void
freelist_free_later(struct freelist *l, uint64_t base, uint64_t nbytes)
{
    dstack_push(&free_later, base);
    dstack_push(&free_later, nbytes);
}

int
freelist_commit(struct freelist *l)
{
    int r;

    while (!dstack_empty(&free_later)) {
	uint64_t nbytes = dstack_pop(&free_later);
	uint64_t base = dstack_pop(&free_later);
	if ((r = freelist_free(l, base, nbytes)) < 0)
	    return r;
    }

    return 0;
}

void
freelist_deserialize(struct freelist *l, void *buf)
{
    memcpy(l, buf, sizeof(struct freelist));
    // XXX
    dstack_init(&free_later);
}

void
freelist_serialize(void *buf, struct freelist *l)
{
    memcpy(buf, l, sizeof(*l));
}

void
freelist_init(struct freelist *l, uint64_t base, uint64_t nbytes)
{
    int r;

    uint32_t frm_bytes;
    frm_bytes = frm_init(&l->chunk_frm, base);
    base += frm_bytes;
    nbytes -= frm_bytes;

    frm_bytes = frm_init(&l->offset_frm, base);
    base += frm_bytes;
    nbytes -= frm_bytes;

    if ((r = freelist_insert(l, base, nbytes)) < 0)
	panic("freelist_init: insert failed: %s", e2s(r));

    l->free = nbytes;

    // XXX
    dstack_init(&free_later);
}
