#ifndef _BTREE_BITER_H_
#define _BTREE_BITER_H_

typedef struct biter
{
    btree_desc_t bi_td;
    bnode_desc_t bi_nd;
    uint16_t bi_key_ndx;
} biter_t;

int biter_init(btree_desc_t *td, biter_t *ti);
int biter_next(biter_t *ti, bkey_t *kstore, bval_t *vstore);

#endif
