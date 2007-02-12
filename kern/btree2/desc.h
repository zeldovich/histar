#ifndef _BTREE_DESC_H_
#define _BTREE_DESC_H_

#include <btree/sys.h>

enum { null_offset = 0 };

typedef struct btree_desc 
{
    offset_t bt_off;
    offset_t bt_root;
    // XXX left leaf

    uint16_t bt_n;
    uint16_t bt_key_max;
    uint16_t bt_ptr_max;  
    uint16_t bt_val_max;  

    uint16_t bt_val_min;
    uint16_t bt_key_min;

    uint16_t bt_key_len;
    uint16_t bt_val_len;

    uint32_t bt_key_sz;
    uint32_t bt_val_sz;

    uint32_t bt_key_tot;
    uint32_t bt_ptr_tot;
    uint32_t bt_val_tot;
    uint32_t bt_node_tot;
    
    uint32_t bt_key_doff;
    uint32_t bt_ptr_doff;
    uint32_t bt_val_doff;
} btree_desc_t;

typedef struct bnode_desc
{
    uint8_t bn_isleaf;
    
    uint16_t bn_key_cnt;
    
    offset_t bn_off;
} bnode_desc_t;

void btree_desc_print(btree_desc_t *bd);
void bnode_desc_print(bnode_desc_t *bn);

#endif
