#include <btree/btree.h>
#include <btree/bxutil.h>
#include <btree/bnode.h>
#include <btree/biter.h>
#include <btree/error.h>

int 
biter_init(btree_desc_t *td, biter_t *ti)
{
    return_error(sys_clear());

    memset(ti, 0, sizeof(*ti));
    memcpy(&ti->bi_td, td, sizeof(ti->bi_td));
    if (td->bt_root != null_offset) {
	// XXX left leaf
	bnode_desc_t nd;
	bnode_read(td->bt_root, &nd);
	while(!nd.bn_isleaf)
	    return_error(bnode_child_read(td, &nd, 0, &nd));
	memcpy(&ti->bi_nd, &nd, sizeof(ti->bi_nd));
    }
    return 0;
}

int 
biter_next(biter_t *ti, bkey_t *kstore, bval_t *vstore)
{
    return_error(sys_clear());
    
    btree_desc_t *td = &ti->bi_td;
    bnode_desc_t *nd = &ti->bi_nd;

    if (nd->bn_off == null_offset)
	return 0;

    assert(ti->bi_key_ndx < nd->bn_key_cnt);
    bkey_read_one(td, nd, ti->bi_key_ndx, kstore);
    bval_read_one(td, nd, ti->bi_key_ndx, vstore);
    ti->bi_key_ndx++;

    if (ti->bi_key_ndx == nd->bn_key_cnt) {
	offset_t sib;
	bsib_read(td, nd, &sib);

	if (sib)
	    bnode_read(sib, nd);
	else
	    memset(nd, 0, sizeof(*nd));
	ti->bi_key_ndx = 0;
    }

    return 1;
}

