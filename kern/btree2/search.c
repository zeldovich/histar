#include <btree/btree.h>
#include <btree/bnode.h>
#include <btree/bxutil.h>
#include <btree/sys.h>
#include <btree/error.h>

static int
_search(btree_desc_t *td, bnode_desc_t *rd, bkey_t *key, 
	bkey_t *kstore, bval_t *vstore)
{
    if (rd->bn_isleaf) {
	uint16_t i;
	if (!bnode_key_index(td, rd, key, &i))
	    return 0;
	bkey_read_one(td, rd, i, kstore);
	bval_read_one(td, rd, i, vstore);
	return 1;
    } else {
	uint16_t i;
	return_error(bnode_child_index(td, rd, key, &i));

	bnode_desc_t cd;
	return_error(bnode_child_read(td, rd, i, &cd));
	return _search(td, &cd, key, kstore, vstore);
    }
}

int 
btree_search(btree_desc_t *td, bkey_t *key, bkey_t *kstore, bval_t *vstore)
{
    return_error(sys_clear());

    bnode_desc_t rd;
    if (td->bt_root == null_offset)
	return 0;

    bnode_read(td->bt_root, &rd);
    return _search(td, &rd, key, kstore, vstore);
}
