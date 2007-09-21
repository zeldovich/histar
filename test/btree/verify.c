#include <btree/btree.h>
#include <btree/bnode.h>
#include <btree/error.h>
#include <btree/bxutil.h>

void
btree_verify(btree_desc_t *td)
{
    assert_error(sys_clear());

    if (td->bt_root == null_offset)
	return;

    bnode_desc_t nd;
    bnode_read(td->bt_root, &nd);
    bkey_t lo[td->bt_key_len];
    bkey_t hi[td->bt_key_len];
    memset(lo, 0x00000000, td->bt_key_sz);
    memset(hi, 0xFFFFFFFF, td->bt_key_sz);
    bnode_verify_keys(td, &nd, lo, hi);
}
