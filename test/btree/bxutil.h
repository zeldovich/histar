#ifndef _BTREE_BXUTIL_H_
#define _BTREE_BXUTIL_H_

#if !defined(BTREE_PRIV)
#error "#include <btree/btree.h> instead??"
#endif

void bkey_read_one(btree_desc_t *td, bnode_desc_t *nd, uint16_t i, bkey_t *key);
void bval_read_one(btree_desc_t *td, bnode_desc_t *nd, uint16_t i, bval_t *val);

void bsib_read(btree_desc_t *td, bnode_desc_t *nd, offset_t *o);
void bsib_write(btree_desc_t *td, bnode_desc_t *nd, offset_t *o);

void btree_write(btree_desc_t *td);

void bnode_read(offset_t roff, bnode_desc_t *nd);
void bnode_write(bnode_desc_t *nd);

#endif
