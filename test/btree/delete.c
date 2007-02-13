#include <btree/btree.h>
#include <btree/bnode.h>
#include <btree/error.h>
#include <btree/bxutil.h>

static int 
_borrow_right_leaf(btree_desc_t *td, bnode_desc_t *rd, bnode_desc_t *pd,
		   int16_t rndx)
{
    assert(rndx <= td->bt_key_max);

    // far right sib?
    if (rndx == pd->bn_key_cnt)
	return 0;

    bnode_desc_t sd;
    return_error(bnode_child_read(td, pd, rndx + 1, &sd));
    
    assert(sd.bn_key_cnt >= td->bt_val_min);

    // nothing to borrow
    if (sd.bn_key_cnt == td->bt_val_min)
	return 0;
    
    return_error(bnode_right_leaf_borrow(td, rd, &sd, pd, rndx));
    return 1;
}

static int 
_borrow_left_leaf(btree_desc_t *td, bnode_desc_t *rd, bnode_desc_t *pd,
		  int16_t rndx)
{
    assert(rndx <= td->bt_key_max);

    // far left sib?
    if (rndx == 0)
	return 0;

    bnode_desc_t sd;
    return_error(bnode_child_read(td, pd, rndx - 1, &sd));

    assert(sd.bn_key_cnt >= td->bt_val_min);
    
    // nothing to borrow
    if (sd.bn_key_cnt == td->bt_val_min)
	return 0;

    return_error(bnode_left_leaf_borrow(td, rd, &sd, pd, rndx));
    return 1;
}

static int 
_merge_right_leaf(btree_desc_t *td, bnode_desc_t *rd, bnode_desc_t *pd,
		  int16_t rndx)
{
    assert(rndx <= td->bt_key_max);

    // far right sib?
    if (rndx == pd->bn_key_cnt)
	return 0;
    
    bnode_desc_t sd;
    return_error(bnode_child_read(td, pd, rndx + 1, &sd));
    
    assert(sd.bn_key_cnt == td->bt_val_min);
    
    return_error(bnode_right_leaf_merge(td, rd, &sd, pd, rndx));
    return 1;
}

static int 
_merge_left_leaf(btree_desc_t *td, bnode_desc_t *rd, bnode_desc_t *pd,
		 int16_t rndx)
{
    assert(rndx <= td->bt_key_max);

    // far left sib?
    if (rndx == 0)
	return 0;

    bnode_desc_t sd;
    return_error(bnode_child_read(td, pd, rndx - 1, &sd));
    
    assert(sd.bn_key_cnt == td->bt_val_min);
    
    return_error(bnode_left_leaf_merge(td, &sd, rd, pd, rndx));
    return 1;
}

static int
_delete_from_leaf(btree_desc_t *td, bnode_desc_t *rd, bnode_desc_t *pd,
		  uint16_t rndx, bkey_t *key)
{
    int r;
    uint16_t i;
    return_error(r = bnode_key_index(td, rd, key, &i));
    if (!r)
	return -E_NOT_FOUND;

    return_error(bnode_leaf_del(td, rd, i));

    // check if root
    if (!pd)
	return 0;

    if (rd->bn_key_cnt < td->bt_val_min) {
	return_error(r = _borrow_right_leaf(td, rd, pd, rndx));
	if (!r)
	    return_error(r = _borrow_left_leaf(td, rd, pd, rndx));
	if (!r)
	    return_error(r = _merge_right_leaf(td, rd, pd, rndx));
	if (!r)
	    return_error(r = _merge_left_leaf(td, rd, pd, rndx));
	assert(r);
    }
    
    return 0;
}

static int
_borrow_right_int(btree_desc_t *td, bnode_desc_t *leftd, bnode_desc_t *pd, 
		  bchild_ndx_t left_ndx)
{
    assert(left_ndx <= td->bt_key_max);
    
    // far right sib?
    if (left_ndx == pd->bn_key_cnt)
	return 0;
    
    bnode_desc_t rightd;
    return_error(bnode_child_read(td, pd, left_ndx + 1, &rightd));
    
    assert(rightd.bn_key_cnt >= td->bt_key_min);

    // nothing to borrow
    if (rightd.bn_key_cnt == td->bt_key_min)
	return 0;
    
    return_error(bnode_right_int_borrow(td, leftd, &rightd, pd, left_ndx + 1));
    return 1;
}

static int
_borrow_left_int(btree_desc_t *td, bnode_desc_t *rightd, bnode_desc_t *pd, 
		 bchild_ndx_t right_ndx)
{
    assert(right_ndx <= td->bt_key_max);

    // far left sib?
    if (right_ndx == 0)
	return 0;

    bnode_desc_t leftd;
    return_error(bnode_child_read(td, pd, right_ndx - 1, &leftd));

    assert(leftd.bn_key_cnt >= td->bt_key_min);
    
    // nothing to borrow
    if (leftd.bn_key_cnt == td->bt_key_min)
	return 0;

    return_error(bnode_left_int_borrow(td, &leftd, rightd, pd, right_ndx));
    return 1;
}

static int
_merge_right_int(btree_desc_t *td, bnode_desc_t *leftd, bnode_desc_t *pd, 
		 bchild_ndx_t left_ndx)
{
    assert(left_ndx <= td->bt_key_max);

    // far right sib?
    if (left_ndx == pd->bn_key_cnt)
	return 0;
    
    bnode_desc_t rightd;
    return_error(bnode_child_read(td, pd, left_ndx + 1, &rightd));
    
    assert(rightd.bn_key_cnt == td->bt_key_min);
    assert(leftd->bn_key_cnt + 1 == td->bt_key_min);
    
    return_error(bnode_int_merge(td, leftd, &rightd, pd, left_ndx + 1));
    return 1;
}

static int
_merge_left_int(btree_desc_t *td, bnode_desc_t *rightd, bnode_desc_t *pd, 
		bchild_ndx_t right_ndx)
{
    assert(right_ndx <= td->bt_key_max);
    
    // far left sib?
    if (right_ndx == 0)
	return 0;

    bnode_desc_t leftd;
    return_error(bnode_child_read(td, pd, right_ndx - 1, &leftd));
    
    assert(leftd.bn_key_cnt == td->bt_key_min);
    assert(rightd->bn_key_cnt + 1 == td->bt_key_min);
    
    return_error(bnode_int_merge(td, &leftd, rightd, pd, right_ndx));
    return 1;
}

static int
_fix_int(btree_desc_t *td, bnode_desc_t *rd, bnode_desc_t *pd, uint16_t rndx) 
{
    // check if root
    if (!pd)
	return 0;

    int r;
    return_error(r = _borrow_right_int(td, rd, pd, rndx));
    if (!r)
	return_error(r = _borrow_left_int(td, rd, pd, rndx));
    if (!r)
	return_error(r = _merge_right_int(td, rd, pd, rndx));
    if (!r)
	return_error(r = _merge_left_int(td, rd, pd, rndx));
    assert(r);

    return 0;
}

static int
_delete(btree_desc_t *td, bnode_desc_t *rd, bnode_desc_t *pd, 
	int16_t rndx, bkey_t *key)
{
    if (rd->bn_isleaf) {
	return_error(_delete_from_leaf(td, rd, pd, rndx, key));
    } else {
	uint16_t i;
	return_error(bnode_child_index(td, rd, key, &i));
	bnode_desc_t cd;
	return_error(bnode_child_read(td, rd, i, &cd));
	
	return_error(_delete(td, &cd, rd, i, key));

	if (rd->bn_key_cnt < td->bt_key_min)
	    return_error(_fix_int(td, rd, pd, rndx));
    }
    return 0;
}

int
btree_delete(btree_desc_t *td, bkey_t *key)
{
    return_error(sys_clear());

    if (td->bt_root == null_offset)
	return -E_NOT_FOUND;

    bnode_desc_t rd;
    bnode_read(td->bt_root, &rd);
    
    return_error(_delete(td, &rd, 0, 0, key));

    if (rd.bn_key_cnt == 0) {
	if (!rd.bn_isleaf) {
	    bnode_desc_t cd;
	    return_error(bnode_child_read(td, &rd, 0, &cd));
	    td->bt_root = cd.bn_off;
	    return_error(sys_free(rd.bn_off));
	} else {
	    td->bt_root = null_offset;
	    return_error(sys_free(rd.bn_off));
	}
    }

    btree_write(td);
    return sys_flush();
}
