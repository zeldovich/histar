/*
 * XXX:
 * - might want to push a lot of the bx operations into sys.h -- esp
     anything that uses an array off the stack
 * - add less-than-equal and greater-than-equal search
 * - can probably remove most of the return_error stuff now
 * - when least value is deleted from leaf, new least value is not raised
 * - don't track left leaf -- iter init is costly
 * - for values > 1 should borrow space from the keys array to fit more
 *   values in the leaf
 * - only borrow from siblings, don't try cousins
 * - bnode_int_ins semantics are a little wierd
 */

#ifndef _BTREE_BTREE_H_
#define _BTREE_BTREE_H_

#include <btree/sys.h>
#include <btree/desc.h>
#include <btree/biter.h>

int btree_alloc(uint16_t n, uint16_t key_sz, uint16_t val_sz, btree_desc_t *bd);
int btree_free(btree_desc_t *td);

int btree_insert(btree_desc_t *bd, bkey_t *key, bval_t *val);
int btree_delete(btree_desc_t *bd, bkey_t *key);
int btree_search(btree_desc_t *td, bkey_t *key, bkey_t *kstore, bval_t *vstore);

void btree_print(btree_desc_t *td);
void btree_verify(btree_desc_t *td);

#endif
