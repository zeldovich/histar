#include <btree/btree.h>
#include <btree/bnode.h>
#include <btree/sys.h>
#include <btree/error.h>
#include <btree/bxutil.h>

static int
_add_to_int(btree_desc_t *td, bnode_desc_t *rd,
	    bkey_t *key, offset_t off,
	    bkey_t *split_key, offset_t *split_off)
{
    if (rd->bn_key_cnt < td->bt_key_max) {
	return_error(bnode_int_ins(td, rd, key, off));
	*split_off = null_offset;
    } else {
	bnode_desc_t nd;
	return_error(bnode_alloc(td, &nd));

	return_error(bnode_int_split(td, rd, &nd,
				     key, off, 
				     split_key, split_off));
	
    }
    return 0;
}

static int
_add_to_leaf(btree_desc_t *td, bnode_desc_t *rd, bkey_t *key, bval_t *val,
	     bkey_t *split_key, offset_t *split_off)
{
    int r;
    uint16_t i;
    return_error(r = bnode_key_index(td, rd, key, &i));
    if (r)
	return -E_INVAL;
    
    if (rd->bn_key_cnt < td->bt_val_max) {
	return_error(bnode_leaf_ins(td, rd, i, key, val));
	*split_off = null_offset;
    } else {
	bnode_desc_t nd;
	return_error(bnode_alloc(td, &nd));
	nd.bn_isleaf = 1;
	return_error(bnode_leaf_split(td, rd, &nd, key, val, 
				      split_key, split_off));
    }
    return 0;
}

static int
_insert(btree_desc_t *td, bnode_desc_t *rd, bkey_t *key, bval_t *val,
	bkey_t *split_key, offset_t *split_off)
{
    if (rd->bn_isleaf) {
	return_error(_add_to_leaf(td, rd, key, val, split_key, split_off));	
    } else {
	uint16_t i;
	return_error(bnode_child_index(td, rd, key, &i));
	bnode_desc_t cd;
	return_error(bnode_child_read(td, rd, i, &cd));

	offset_t split_off1 = null_offset;
	bkey_t split_key1[td->bt_key_len];
	return_error(_insert(td, &cd, key, val, split_key1, &split_off1));
	
	if (split_off1 != null_offset) {
	    return_error(_add_to_int(td, rd,
				     split_key1, split_off1, 
				     split_key, split_off));
	}
    }
    return 0;
}

int
btree_insert(btree_desc_t *td, bkey_t *key, bval_t *val)
{
    return_error(sys_clear());

    bnode_desc_t rd;
    if (td->bt_root == null_offset) {
	return_error(bnode_alloc(td, &rd));
	rd.bn_isleaf = 1;
	bnode_write(&rd);
	td->bt_root = rd.bn_off;
    } else
	bnode_read(td->bt_root, &rd);
    
    offset_t split_off = null_offset;
    bkey_t split_key[td->bt_key_len];

    return_error(_insert(td, &rd, key, val, split_key, &split_off));

    if (split_off != null_offset) {
	bnode_desc_t rd_new;
	return_error(bnode_alloc(td, &rd_new));
	assert_error(bnode_root_ins(td, &rd_new, 
				    split_key, rd.bn_off, split_off));
	td->bt_root = rd_new.bn_off;
    }

    btree_write(td);
    return sys_flush();
}
