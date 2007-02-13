#include <btree/btree.h>
#include <btree/bxutil.h>

void
btree_write(btree_desc_t *td)
{
    sys_write(sizeof(*td), td->bt_off, td);
}

void 
bnode_read(offset_t off, bnode_desc_t *bn)
{
    sys_read(sizeof(*bn), off, bn);
}

void 
bnode_write(bnode_desc_t *nd)
{
    sys_write(sizeof(*nd), nd->bn_off, nd);
}

void 
bkey_read_one(btree_desc_t *td, bnode_desc_t *nd, uint16_t i, bkey_t *key)
{
    offset_t off = (nd->bn_off + td->bt_key_doff) + (i * td->bt_key_sz);
    sys_read(td->bt_key_sz, off, key);
}

void
bval_read_one(btree_desc_t *td, bnode_desc_t *nd, uint16_t i, bval_t *val)
{
    offset_t off = (nd->bn_off + td->bt_val_doff) + (i * td->bt_val_sz);
    sys_read(td->bt_val_sz, off, val);
}

void
bsib_read(btree_desc_t *td, bnode_desc_t *nd, offset_t *o)
{
    offset_t off = (nd->bn_off + td->bt_ptr_doff) + 
	(td->bt_key_max * sizeof(offset_t));
    sys_read(sizeof(offset_t), off, o);
}

void
bsib_write(btree_desc_t *td, bnode_desc_t *nd, offset_t *o)
{
    offset_t off = (nd->bn_off + td->bt_ptr_doff) + 
	(td->bt_key_max * sizeof(offset_t));
    sys_write(sizeof(offset_t), off, o);
}

#if 0
static int
bptr_write_one(btree_desc_t *td, bnode_desc_t *nd, uint16_t i, offset_t *o)
{
    offset_t off = (nd->bn_off + td->bt_ptr_doff) + (i * sizeof(offset_t));
    return sys_write(sizeof(offset_t), off, o);
}

static void
bval_copy(btree_desc_t *td, bval_t *dst, bval_t *src)
{
    memcpy(dst, src, td->bt_val_sz);
}

static bkey_t *
bvali(btree_desc_t *td, void *vals, uint32_t i)
{
    return (bval_t *) &((char *)vals)[td->bt_val_sz * i];
}
#endif
