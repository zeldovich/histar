#ifndef _BTREE_BNODE_H_
#define _BTREE_BNODE_H_

#if !defined(BTREE_PRIV)
#error "#include <btree/btree.h> instead??"
#endif

#include <btree/desc.h>

/*
 * Alloc/free 
 */
int bnode_alloc(btree_desc_t *bt, bnode_desc_t *nd);
int bnode_rec_free(btree_desc_t *bt, bnode_desc_t *nd);

/*
 * Search/traverse
 */
int bnode_key_index(btree_desc_t *td, bnode_desc_t *nd,
		    bkey_t *key, uint16_t *i);
int bnode_child_index(btree_desc_t *td, bnode_desc_t *nd,
		    bkey_t *key, uint16_t *i);
int bnode_child_read(btree_desc_t *td, bnode_desc_t *nd, 
		     uint16_t i, bnode_desc_t *cd);

/* 
 * Root
 */ 
/* Insertion */
int bnode_root_ins(btree_desc_t *td, bnode_desc_t *rd,
		   bkey_t *key, offset_t off0, offset_t off1);

/* 
 * Internal 
*/
/* Instertion */
int bnode_int_ins(btree_desc_t *td, bnode_desc_t *rd,
		  bkey_t *key, offset_t off);
int bnode_int_split(btree_desc_t *td, bnode_desc_t *rd, bnode_desc_t *nd, 
		    bkey_t *key, offset_t off, 
		    bkey_t *split_key, offset_t *split_off);
/* Deletion */
int bnode_right_int_borrow(btree_desc_t *td, 
			   bnode_desc_t *dstd, bnode_desc_t *srcd, 
			   bnode_desc_t *pard, uint16_t key_ndx);
int bnode_left_int_borrow(btree_desc_t *td, 
			  bnode_desc_t *dstd, bnode_desc_t *srcd, 
			  bnode_desc_t *pard, uint16_t key_ndx);
int bnode_right_int_merge(btree_desc_t *td, 
			  bnode_desc_t *dstd, bnode_desc_t *srcd, 
			  bnode_desc_t *pard, uint16_t key_ndx);
int bnode_left_int_merge(btree_desc_t *td, 
			 bnode_desc_t *dstd, bnode_desc_t *srcd, 
			 bnode_desc_t *pard, uint16_t key_ndx);

/*
 * Leaf
 */
/* Insertion */
int bnode_leaf_ins(btree_desc_t *td, bnode_desc_t *rd, uint16_t i,
		   bkey_t *key, bval_t *val);
int bnode_leaf_split(btree_desc_t *td, bnode_desc_t *rd, bnode_desc_t *nd, 
		     bkey_t *key, bval_t *val,
		     bkey_t *split_key, offset_t *split_off);
/* Deletion */
int bnode_leaf_del(btree_desc_t *td, bnode_desc_t *rd, uint16_t i);
int bnode_right_leaf_borrow(btree_desc_t *td, 
			    bnode_desc_t *dstd, bnode_desc_t *srcd, 
			    bnode_desc_t *pard, uint16_t key_ndx);
int bnode_left_leaf_borrow(btree_desc_t *td, 
			   bnode_desc_t *dstd, bnode_desc_t *srcd, 
			   bnode_desc_t *pard, uint16_t key_ndx);
int bnode_right_leaf_merge(btree_desc_t *td, 
			   bnode_desc_t *dstd, bnode_desc_t *srcd, 
			   bnode_desc_t *pard, uint16_t key_ndx);
int bnode_left_leaf_merge(btree_desc_t *td, 
			  bnode_desc_t *dstd, bnode_desc_t *srcd, 
			  bnode_desc_t *pard, uint16_t key_ndx);

/*
 * Debug
 */
void bnode_print(btree_desc_t *td, bnode_desc_t *nd);
void bnode_verify_keys(btree_desc_t *td, bnode_desc_t *nd, 
		       bkey_t *lo, bkey_t *hi);

#endif
