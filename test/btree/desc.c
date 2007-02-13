#include <btree/desc.h>

void 
btree_desc_print(btree_desc_t *td)
{
    wprintf("btree_desc (off %"OFF_F") (root %"OFF_F") \n"
	    " (n %d) (key_max %d) (ptr_max %d) (val_max %d)\n"
	    " (val_min %d)\n"
	    " (key_len %d) (val_len %d)\n"
	    " (key_sz %d) (val_sz %d)\n"
	    " (key_doff %d) (ptr_doff %d) (val_doff %d)\n"
	    " (key_tot %d) (prt_tot %d) (val_tot %d) (node_tot %d)\n",
	    td->bt_off, td->bt_root,
	    td->bt_n, td->bt_key_max, td->bt_ptr_max, td->bt_val_max, 
	    td->bt_val_min,
	    td->bt_key_len, td->bt_val_len,
	    td->bt_key_sz, td->bt_val_sz,
	    td->bt_key_doff, td->bt_ptr_doff, td->bt_val_doff,
	    td->bt_key_tot, td->bt_ptr_tot, td->bt_val_tot, td->bt_node_tot);
}

void 
bnode_desc_print(bnode_desc_t *bn)
{
    wprintf("NODE DESC [%"OFF_F"] (isleaf %d) (key_cnt %d)\n", 
	    bn->bn_off, bn->bn_isleaf, bn->bn_key_cnt);
}
