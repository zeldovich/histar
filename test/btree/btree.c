#include <btree/btree.h>
#include <btree/bxutil.h>
#include <btree/bnode.h>
#include <btree/sys.h>
#include <btree/error.h>

int
btree_alloc(uint16_t n, uint16_t key_len, uint16_t val_len, btree_desc_t *td)
{
    if (n < 3)
	return -E_INVAL;

    return_error(sys_clear());
    
    offset_t off;
    return_error(sys_alloc(sizeof(*td), &off));
    memset(td, 0, sizeof(*td));

    td->bt_key_len = key_len;
    td->bt_val_len = val_len;

    td->bt_key_sz = key_len * sizeof(bkey_t);
    td->bt_val_sz = val_len * sizeof(bval_t);
    
    td->bt_n = n;
    td->bt_key_max = n;
    td->bt_ptr_max = n + 1;
    td->bt_val_max = n / (td->bt_val_sz / sizeof(offset_t));

    td->bt_key_min = td->bt_key_max / 2;
    td->bt_val_min = td->bt_val_max - (td->bt_val_max / 2);
    
    td->bt_key_tot = td->bt_key_sz * td->bt_key_max;
    td->bt_ptr_tot = sizeof(offset_t) * td->bt_ptr_max;
    td->bt_val_tot = td->bt_val_sz * td->bt_val_max;
    td->bt_node_tot = sizeof(bnode_desc_t) +
	td->bt_key_tot + td->bt_ptr_tot;
  
    td->bt_key_doff = sizeof(bnode_desc_t);
    td->bt_ptr_doff = td->bt_key_doff + td->bt_key_tot;
    td->bt_val_doff = td->bt_ptr_doff;
    
    td->bt_off = off;
    td->bt_root = null_offset;

    // have room for sib ptr in leaf
    assert(td->bt_val_tot <= (td->bt_ptr_tot - sizeof(offset_t)));
    
    return sys_flush();
}

int
btree_free(btree_desc_t *td)
{
    return_error(sys_clear());

    bnode_desc_t rd;
    if (td->bt_root == null_offset)
	return 0;
    
    bnode_read(td->bt_root, &rd);
    return_error(bnode_rec_free(td, &rd));
    return_error(sys_free(td->bt_off));
    return sys_flush();
}

static void
_print(btree_desc_t *td, bnode_desc_t *nd, uint16_t space)
{
    for (uint16_t i = 0; i < space; i++)
	wprintf(" ");

    bnode_print(td, nd);
    
    if (nd->bn_isleaf)
	return;
    
    for (uint16_t i = 0; i < nd->bn_key_cnt + 1; i++) {
	bnode_desc_t rd;
	assert_error(bnode_child_read(td, nd, i, &rd));
	_print(td, &rd, space + 1);
    }
}

void
btree_print(btree_desc_t *td)
{
    bnode_desc_t rd;
    if (td->bt_root == null_offset) {
	wprintf(" [ null root ]\n");
	return;
    }
    bnode_read(td->bt_root, &rd);
    _print(td, &rd, 0);
}
