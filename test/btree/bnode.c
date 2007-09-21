#include <btree/btree.h>
#include <btree/bnode.h>
#include <btree/sys.h>
#include <btree/error.h>
#include <btree/bxutil.h>

static void
bptr_read_one(btree_desc_t *td, bnode_desc_t *nd, uint16_t i, offset_t *o)
{
    offset_t off = (nd->bn_off + td->bt_ptr_doff) + (i * sizeof(offset_t));
    sys_read(sizeof(offset_t), off, o);
}

static void
bkey_write_one(btree_desc_t *td, bnode_desc_t *nd, uint16_t i, bkey_t *key)
{
    offset_t off = (nd->bn_off + td->bt_key_doff) + (i * td->bt_key_sz);
    sys_write(td->bt_key_sz, off, key);
}

static void
bkeys_read(btree_desc_t *td, bnode_desc_t *nd, void *keys)
{
    sys_read(td->bt_key_tot, nd->bn_off + td->bt_key_doff, keys);
}

static void
bkeys_write(btree_desc_t *td, bnode_desc_t *nd, void *keys)
{
    sys_write(td->bt_key_tot, nd->bn_off + td->bt_key_doff, keys);
}

static void
bvals_read(btree_desc_t *td, bnode_desc_t *nd, void *vals)
{
    sys_read(td->bt_val_tot, nd->bn_off + td->bt_val_doff, vals);
}

static void
bvals_write(btree_desc_t *td, bnode_desc_t *nd, void *vals)
{
    sys_write(td->bt_val_tot, nd->bn_off + td->bt_val_doff, vals);
}

static void
bptrs_read(btree_desc_t *td, bnode_desc_t *nd, void *ptrs)
{
    sys_read(td->bt_ptr_tot, nd->bn_off + td->bt_ptr_doff, ptrs);
}

static void
bptrs_write(btree_desc_t *td, bnode_desc_t *nd, void *ptrs)
{
    sys_write(td->bt_ptr_tot, nd->bn_off + td->bt_ptr_doff, ptrs);
}

static void
bkey_copy(btree_desc_t *td, bkey_t *dst, bkey_t *src)
{
    memcpy(dst, src, td->bt_key_sz);
}

static bkey_t *
bkeyi(btree_desc_t *td, void *keys, uint32_t i)
{
    return (bkey_t *) &((char *)keys)[td->bt_key_sz * i];
}

static int
bkey_cmp(btree_desc_t *td, bkey_t *k0, bkey_t *k1)
{
    for (uint32_t i = 0; i < (td->bt_key_sz / sizeof(bkey_t)); i++) {
	if (k0[i] > k1[i])
	    return 1;
	if (k0[i] < k1[i])
	    return -1;
    }
    return 0;
}

static void
bptr_del(uint16_t i, void *ptrs, uint16_t ptr_cnt)
{
    offset_t dst = i * sizeof(offset_t);
    offset_t src = dst + sizeof(offset_t);

    void *dst_addr = (void *)((offset_t)ptrs + dst);
    void *src_addr = (void *)((offset_t)ptrs + src);

    uint64_t n = (ptr_cnt - 1 - i) * sizeof(offset_t);
    memmove(dst_addr, src_addr, n);
}

static void
bkey_del(btree_desc_t *td, uint16_t i, void *keys, uint16_t key_cnt)
{
    offset_t dst = i * td->bt_key_sz;
    offset_t src = dst + td->bt_key_sz;

    void *dst_addr = (void *)((offset_t)keys + dst);
    void *src_addr = (void *)((offset_t)keys + src);

    uint64_t n = (key_cnt - 1 - i) * td->bt_key_sz;
    memmove(dst_addr, src_addr, n);
}

static void
bkey_ins(btree_desc_t *td, bkey_t *key, uint16_t i,
	 void *keys, uint16_t key_cnt)
{
    offset_t src = i * td->bt_key_sz;
    offset_t dst = src + td->bt_key_sz; 
    
    void *src_addr = (void *)((offset_t)keys + src);
    void *dst_addr = (void *)((offset_t)keys + dst);

    uint64_t n = (key_cnt - i) * td->bt_key_sz;

    memmove(dst_addr, src_addr, n);
    memcpy(src_addr, key, td->bt_key_sz);
}

static void
bval_del(btree_desc_t *td, uint16_t i, void *vals, uint16_t val_cnt)
{
    offset_t dst = i * td->bt_val_sz;
    offset_t src = dst + td->bt_val_sz;

    void *dst_addr = (void *)((offset_t)vals + dst);
    void *src_addr = (void *)((offset_t)vals + src);

    uint64_t n = (val_cnt - 1 - i) * td->bt_val_sz;
    memmove(dst_addr, src_addr, n);
}

static void
bkeys_merge(btree_desc_t *td, 
	    void *keys0, uint16_t n0, 
	    void *keys1, uint16_t n1)
{
    offset_t dst = n0 * td->bt_key_sz;
    void *dst_addr = (void *)((offset_t)keys0 + dst);
    
    uint64_t n = n1 * td->bt_key_sz;
    
    memcpy(dst_addr, keys1, n);
}

static void
bvals_merge(btree_desc_t *td, 
	    void *vals0, uint16_t n0, 
	    void *vals1, uint16_t n1)
{
    offset_t dst = n0 * td->bt_val_sz;
    void *dst_addr = (void *)((offset_t)vals0 + dst);
    
    uint64_t n = n1 * td->bt_val_sz;
    
    memcpy(dst_addr, vals1, n);
}

static void
bptrs_merge(void *ptrs0, uint16_t n0, 
	    void *ptrs1, uint16_t n1)
{
    offset_t dst = n0 * sizeof(offset_t);
    void *dst_addr = (void *)((offset_t)ptrs0 + dst);
    
    uint64_t n = n1 * sizeof(offset_t);
    
    memcpy(dst_addr, ptrs1, n);
}

static void
bval_ins(btree_desc_t *td, bval_t *val, uint16_t i, 
	 void *vals, uint16_t val_cnt)
{
    offset_t src = i * td->bt_val_sz;
    offset_t dst = src + td->bt_val_sz; 
    
    void *src_addr = (void *)((offset_t)vals + src);
    void *dst_addr = (void *)((offset_t)vals + dst);

    uint64_t n = (val_cnt - i) * td->bt_val_sz;
    
    memmove(dst_addr, src_addr, n);
    memcpy(src_addr, val, td->bt_val_sz);
}

static void
bptr_ins(offset_t off, uint16_t i, void *ptrs, uint16_t ptr_cnt)
{
    offset_t src = i * sizeof(offset_t);
    offset_t dst = src + sizeof(offset_t); 
    
    void *src_addr = (void *)((offset_t)ptrs + src);
    void *dst_addr = (void *)((offset_t)ptrs + dst);

    uint64_t n = (ptr_cnt - i) * sizeof(offset_t);
    
    memmove(dst_addr, src_addr, n);
    memcpy(src_addr, &off, sizeof(offset_t));
}

static int
bkey_index(btree_desc_t *td, bnode_desc_t *rd, 
	   bkey_t *key, void *keys, char *found)
{
    uint32_t i = 0;
    for (; i < rd->bn_key_cnt; i++) {
	int c = bkey_cmp(td, bkeyi(td, keys, i), key);
	if (c == 0) {
	    if (found)
		*found = 1;
	    return i;
	}
	else if(0 < c) {
	    if (found)
		*found = 0;
	    return i;
	}
    }

    if (found)
	*found = 0;
    
    return i;
}

static void
bkey_print(btree_desc_t *td, bkey_t *k)
{
    wprintf("%016"BKEY_F, k[0]);
    for (uint32_t i = 1; i < (td->bt_key_sz / sizeof(bkey_t)); i++)
	wprintf("|%016"BKEY_F, k[i]);
}

static int
bkey_int_split(btree_desc_t *td, uint16_t i, bkey_t *key,
	       void *keys0, uint16_t *keys0_cnt, 
	       void *keys1, uint16_t *keys1_cnt,
	       bkey_t *split_key)
{
    uint16_t k1_cnt = (*keys0_cnt) - (*keys0_cnt / 2);
    uint32_t split = (*keys0_cnt) / 2;
    uint16_t k0_cnt = (*keys0_cnt) - k1_cnt;
    
    if (i == split) {
	offset_t src = split * td->bt_key_sz;
	void *src_addr = (void *)((offset_t)keys0 + src);

	uint64_t n1 = k1_cnt * td->bt_key_sz;
	memcpy(keys1, src_addr, n1);
	bkey_copy(td, split_key, key);
    } else if (i < split) {
	offset_t src = split * td->bt_key_sz;
	void *src_addr = (void *)((offset_t)keys0 + src);

	uint64_t n1 = k1_cnt * td->bt_key_sz;
	memcpy(keys1, src_addr, n1);
	
	bkey_copy(td, split_key, bkeyi(td, keys0, split - 1));
	bkey_ins(td, key, i, keys0, k0_cnt);
    } else {
	offset_t src = (split + 1) * td->bt_key_sz;
	void *src_addr = (void *)((offset_t)keys0 + src);
	
	uint64_t n1 = (k1_cnt - 1) * td->bt_key_sz;
	memcpy(keys1, src_addr, n1);
	bkey_ins(td, key, i - split - 1, keys1, k1_cnt - 1);	
	
	bkey_copy(td, split_key, bkeyi(td, keys0, split));
    }

    *keys0_cnt = k0_cnt;
    *keys1_cnt = k1_cnt;
    return 0;
}

static int
bkey_split(btree_desc_t *td, uint16_t i, bkey_t *key,
	   void *keys0, uint16_t *keys0_cnt, void *keys1, uint16_t *keys1_cnt)
{
    uint16_t k1_cnt = (*keys0_cnt + 1) - ((*keys0_cnt + 1) / 2);
    uint32_t split = (*keys0_cnt + 1) / 2;
    uint16_t k0_cnt = (*keys0_cnt + 1) - k1_cnt;


    if (i < split) {
	offset_t src = (split - 1) * td->bt_key_sz;
	void *src_addr = (void *)((offset_t)keys0 + src);
	
	uint64_t n1 = k1_cnt * td->bt_key_sz;
	memcpy(keys1, src_addr, n1);
	bkey_ins(td, key, i, keys0, k0_cnt);
    } else {
	offset_t src = split * td->bt_key_sz;
	void *src_addr = (void *)((offset_t)keys0 + src);
	
	uint64_t n1 = (k1_cnt - 1) * td->bt_key_sz;
	memcpy(keys1, src_addr, n1);
	bkey_ins(td, key, i - split, keys1, k1_cnt - 1);
    }

    *keys0_cnt = k0_cnt;
    *keys1_cnt = k1_cnt;
    return 0;
}

static int
bval_split(btree_desc_t *td, uint16_t i, bval_t *val,
	   void *vals0, uint16_t *vals0_cnt, void *vals1, uint16_t *vals1_cnt)
{
    uint16_t v1_cnt = (*vals0_cnt + 1) - ((*vals0_cnt + 1) / 2);
    uint32_t split = (*vals0_cnt + 1) / 2;
    uint16_t v0_cnt = (*vals0_cnt + 1) - v1_cnt;

    if (i < split) {
	offset_t src = (split - 1) * td->bt_val_sz;
	void *src_addr = (void *)((offset_t)vals0 + src);

	uint64_t n1 = v1_cnt * td->bt_val_sz;
	memcpy(vals1, src_addr, n1);
	bval_ins(td, val, i, vals0, v0_cnt);
    } else {
	offset_t src = split * td->bt_val_sz;
	void *src_addr = (void *)((offset_t)vals0 + src);
	
	uint64_t n1 = (v1_cnt - 1) * td->bt_val_sz;
	memcpy(vals1, src_addr, n1);
	bval_ins(td, val, i - split, vals1, v1_cnt - 1);
    }

    *vals0_cnt = v0_cnt;
    *vals1_cnt = v1_cnt;
    return 0;
}

static int
bptr_split(uint16_t i, offset_t off,
	   void *ptrs0, uint16_t *ptrs0_cnt, 
	   void *ptrs1, uint16_t *ptrs1_cnt)
{
    uint16_t p1_cnt = (*ptrs0_cnt + 1) - ((*ptrs0_cnt + 1) / 2);
    uint32_t split = (*ptrs0_cnt + 1) / 2;
    uint16_t p0_cnt = (*ptrs0_cnt + 1) - p1_cnt;

    if (i < split) {
	offset_t src = (split - 1) * sizeof(offset_t);
	void *src_addr = (void *)((offset_t)ptrs0 + src);

	uint64_t n1 = p1_cnt * sizeof(offset_t);
	memcpy(ptrs1, src_addr, n1);
	bptr_ins(off, i, ptrs0, p0_cnt);
    } else {
	offset_t src = split * sizeof(offset_t);
	void *src_addr = (void *)((offset_t)ptrs0 + src);
	
	uint64_t n1 = (p1_cnt - 1) * sizeof(offset_t);
	memcpy(ptrs1, src_addr, n1);
	bptr_ins(off, i - split, ptrs1, p1_cnt - 1);
    }

    *ptrs0_cnt = p0_cnt;
    *ptrs1_cnt = p1_cnt;
    return 0;
}

/*
 * 
 */

int
bnode_alloc(btree_desc_t *td, bnode_desc_t *nd)
{
    offset_t off;
    return_error(sys_alloc(td->bt_node_tot, &off));
    memset((void *)off, 0, td->bt_node_tot);
    memset(nd, 0, sizeof(*nd));
    nd->bn_off = off;
    bnode_write(nd);
    return 0;
}

int
bnode_rec_free(btree_desc_t *td, bnode_desc_t *nd)
{
    if (!nd->bn_isleaf) {
	for (uint16_t i = 0; i < nd->bn_key_cnt + 1; i++) {
	    bnode_desc_t cd;
	    return_error(bnode_child_read(td, nd, i, &cd));
	    return_error(bnode_rec_free(td, &cd));
	}
    }
    return_error(sys_free(nd->bn_off));
    return 0;
}

int
bnode_child_read(btree_desc_t *td, bnode_desc_t *nd, 
		 uint16_t i, bnode_desc_t *cd)
{
    assert(!nd->bn_isleaf);
    assert(i < (nd->bn_key_cnt + 1));
    offset_t ptrs[td->bt_ptr_max];
    bptrs_read(td, nd, ptrs);
    bnode_read(ptrs[i], cd);
    return 0;
}

int
bnode_child_index(btree_desc_t *td, bnode_desc_t *nd,
		bkey_t *key, uint16_t *i)
{
    assert(!nd->bn_isleaf);
    int r;
    return_error(r = bnode_key_index(td, nd, key, i));
    if (r)
	*i = *i + 1;
    return 0;
}

int
bnode_key_index(btree_desc_t *td, bnode_desc_t *nd,
		bkey_t *key, uint16_t *i)
{
    char keys[td->bt_key_tot];
    bkeys_read(td, nd, keys);
    char f = 0;
    *i = bkey_index(td, nd, key, keys, &f);
    return f;
}

////////

int
bnode_int_ins(btree_desc_t *td, bnode_desc_t *rd, bkey_t *key, offset_t off)
{
    char keys[td->bt_key_tot];
    offset_t ptrs[td->bt_ptr_max];

    bkeys_read(td, rd, keys);
    bptrs_read(td, rd, ptrs);

    char f;
    uint16_t i = bkey_index(td, rd, key, keys, &f);
    assert(!f);
    assert(i <= rd->bn_key_cnt);
    assert(i < td->bt_key_max);
    
    bkey_ins(td, key, i, keys, rd->bn_key_cnt);
    bptr_ins(off, i + 1, ptrs, rd->bn_key_cnt + 1);
    
    bkeys_write(td, rd, keys);
    bptrs_write(td, rd, ptrs);
    rd->bn_key_cnt++;
    
    bnode_write(rd);
    return 0;
}

int 
bnode_root_ins(btree_desc_t *td, bnode_desc_t *rd, bkey_t *key, 
	       offset_t off0, offset_t off1)
{
    assert(rd->bn_key_cnt == 0);
    char keys[td->bt_key_tot];
    offset_t ptrs[td->bt_ptr_max];
    
    bkey_ins(td, key, 0, keys, rd->bn_key_cnt);
    ptrs[0] = off0;
    ptrs[1] = off1;
    
    bkeys_write(td, rd, keys);
    bptrs_write(td, rd, ptrs);
    rd->bn_key_cnt++;
    
    bnode_write(rd);
    return 0;
}

int
bnode_leaf_del(btree_desc_t *td, bnode_desc_t *rd, uint16_t i)
{
    assert(i < td->bt_val_max);
    assert(i < rd->bn_key_cnt);
      
    char keys[td->bt_key_tot];
    char vals[td->bt_val_tot];
    bkeys_read(td, rd, keys);
    bvals_read(td, rd, vals);
    
    bkey_del(td, i, keys, rd->bn_key_cnt);
    bval_del(td, i, vals, rd->bn_key_cnt);
    
    bkeys_write(td, rd, keys);
    bvals_write(td, rd, vals);
    rd->bn_key_cnt--;
    
    bnode_write(rd);
    return 0;
}

int
bnode_leaf_ins(btree_desc_t *td, bnode_desc_t *rd, uint16_t i,
	       bkey_t *key, bval_t *val)
{
    assert(i < td->bt_val_max);

    char keys[td->bt_key_tot];
    char vals[td->bt_val_tot];
    bkeys_read(td, rd, keys);
    bvals_read(td, rd, vals);
    
    bkey_ins(td, key, i, keys, rd->bn_key_cnt);
    bval_ins(td, val, i, vals, rd->bn_key_cnt);
    
    bkeys_write(td, rd, keys);
    bvals_write(td, rd, vals);
    rd->bn_key_cnt++;
    
    bnode_write(rd);
    return 0;
}

int
bnode_int_split(btree_desc_t *td, bnode_desc_t *rd, bnode_desc_t *nd, 
		bkey_t *key, offset_t off, 
		bkey_t *split_key, offset_t *split_off)
{
    assert(rd->bn_key_cnt == td->bt_key_max);
    assert(!rd->bn_isleaf);
    assert(!nd->bn_isleaf);

    char keys0[td->bt_key_tot], keys1[td->bt_key_tot];
    offset_t ptrs0[td->bt_ptr_max], ptrs1[td->bt_ptr_max];
    
    bkeys_read(td, rd, keys0);
    bptrs_read(td, rd, ptrs0);
    
    uint16_t keys0_cnt = rd->bn_key_cnt, keys1_cnt;
    uint16_t ptrs0_cnt = rd->bn_key_cnt + 1, ptrs1_cnt;

    char f;
    uint16_t i = bkey_index(td, rd, key, keys0, &f);

    assert(!f);
    assert(i <= rd->bn_key_cnt); 
    //assert(i < td->bt_key_max);

    return_error(bkey_int_split(td, i, key, 
				keys0, &keys0_cnt, 
				keys1, &keys1_cnt,
				split_key));
    *split_off = nd->bn_off;

    return_error(bptr_split(i + 1, off, 
			    ptrs0, &ptrs0_cnt,
			    ptrs1, &ptrs1_cnt));

    assert((keys0_cnt + 1) == ptrs0_cnt);
    assert((keys1_cnt + 1) == ptrs1_cnt);

    rd->bn_key_cnt = keys0_cnt;
    nd->bn_key_cnt = keys1_cnt;
    
    bkeys_write(td, rd, keys0);
    bptrs_write(td, rd, ptrs0);

    bkeys_write(td, nd, keys1);
    bptrs_write(td, nd, ptrs1);
    
    bnode_write(nd);
    bnode_write(rd);

    return 0;
}


// XXX get rid of me!!!!
static int
bnode_int_del(btree_desc_t *td, bnode_desc_t *nd, 
	      uint16_t key_ndx, uint16_t ptr_ndx)
{
    assert(key_ndx < nd->bn_key_cnt);
    assert(ptr_ndx < nd->bn_key_cnt + 1);
      
    char keys[td->bt_key_tot];
    char ptrs[td->bt_val_tot];
    bkeys_read(td, nd, keys);
    bptrs_read(td, nd, ptrs);
    
    // XXX create sys_array_rem(start, nbytes, i, iwidth_bytes)
    bkey_del(td, key_ndx, keys, nd->bn_key_cnt);
    bptr_del(ptr_ndx, ptrs, nd->bn_key_cnt + 1);
    
    bkeys_write(td, nd, keys);
    bptrs_write(td, nd, ptrs);
    nd->bn_key_cnt--;
    
    bnode_write(nd);
    return 0;
}

int
bnode_int_merge(btree_desc_t *td, 
		bnode_desc_t *leftd, bnode_desc_t *rightd, 
		bnode_desc_t *pard, bchild_ndx_t right_ndx)
{
    assert(!leftd->bn_isleaf);
    assert(!rightd->bn_isleaf);
    assert(right_ndx != 0);
    // assumes dstd is left and srcd is right

    char keys0[td->bt_key_tot], keys1[td->bt_key_tot];
    offset_t ptrs0[td->bt_ptr_max], ptrs1[td->bt_ptr_max];

    bkeys_read(td, leftd, keys0);
    bkeys_read(td, rightd, keys1);
    bptrs_read(td, leftd, ptrs0);
    bptrs_read(td, rightd, ptrs1);

    // move key down from parent to end of keys0
    BKEY_T(td, k);
    bkey_read_one(td, pard, right_ndx - 1, k);
    bkey_copy(td, bkeyi(td, keys0, leftd->bn_key_cnt), k);

    bkeys_merge(td, keys0, leftd->bn_key_cnt + 1, 
		keys1, rightd->bn_key_cnt + 1);
    bptrs_merge(ptrs0, leftd->bn_key_cnt + 1, 
		ptrs1, rightd->bn_key_cnt + 1);

    bkeys_write(td, leftd, keys0);
    bptrs_write(td, leftd, ptrs0);
    
    leftd->bn_key_cnt += (rightd->bn_key_cnt + 1);
    bnode_write(leftd);
    
    // kill right node
    return_error(bnode_int_del(td, pard, right_ndx - 1, right_ndx));
    return_error(sys_free(rightd->bn_off));

    return 0;
}

int
bnode_left_int_borrow(btree_desc_t *td, 
		      bnode_desc_t *leftd, bnode_desc_t *rightd,
		      bnode_desc_t *pard, bchild_ndx_t right_ndx)
{
    BKEY_T(td, k); BKEY_T(td, k1);
    offset_t off;
    bkey_read_one(td, leftd, leftd->bn_key_cnt - 1, k);
    bkey_read_one(td, pard, right_ndx - 1, k1);

    bptr_read_one(td, leftd, leftd->bn_key_cnt, &off);
    
    return_error(bnode_int_del(td, leftd, 
			       leftd->bn_key_cnt - 1, leftd->bn_key_cnt));

    // XXX want to insert in 0
    char keys[td->bt_key_tot];
    offset_t ptrs[td->bt_ptr_max];
    
    bkeys_read(td, rightd, keys);
    bptrs_read(td, rightd, ptrs);

    bkey_ins(td, k1, 0, keys, rightd->bn_key_cnt);
    bptr_ins(off, 0, ptrs, rightd->bn_key_cnt + 1);
    
    bkeys_write(td, rightd, keys);
    bptrs_write(td, rightd, ptrs);
    rightd->bn_key_cnt++;

    bkey_write_one(td, pard, right_ndx - 1, k);

    bnode_write(rightd);
    bnode_write(rightd);
        
    return 0;
}

int
bnode_right_int_borrow(btree_desc_t *td, 
		       bnode_desc_t *leftd, bnode_desc_t *rightd, 
		       bnode_desc_t *pard, bchild_ndx_t right_ndx)
{
    BKEY_T(td, k); BKEY_T(td, k1);
    offset_t off;
    bkey_read_one(td, rightd, 0, k);
    bkey_read_one(td, pard, right_ndx - 1, k1);

    bptr_read_one(td, rightd, 0, &off);
    
    return_error(bnode_int_del(td, rightd, 0, 0));
    // XXX want to insert in key_cnt - 1
    return_error(bnode_int_ins(td, leftd, k1, off));
    bkey_write_one(td, pard, right_ndx - 1, k);
    
    bnode_write(pard);
    // bnode_int_ins calls bnode_write(dstd)

    return 0;
}

static int
bnode_leaf_merge(btree_desc_t *td, 
		 bnode_desc_t *dstd, bnode_desc_t *srcd, 
		 bnode_desc_t *pard, uint16_t key_ndx)
{
    // assumes dstd is left and srcd is right

    char keys0[td->bt_key_tot], keys1[td->bt_key_tot];
    char vals0[td->bt_val_tot], vals1[td->bt_val_tot];

    bkeys_read(td, dstd, keys0);
    bkeys_read(td, srcd, keys1);
    bvals_read(td, dstd, vals0);
    bvals_read(td, srcd, vals1);

    bkeys_merge(td, keys0, dstd->bn_key_cnt, keys1, srcd->bn_key_cnt);
    bvals_merge(td, vals0, dstd->bn_key_cnt, vals1, srcd->bn_key_cnt);

    bkeys_write(td, dstd, keys0);
    bvals_write(td, dstd, vals0);
    
    // set sib
    offset_t sib;
    bsib_read(td, srcd, &sib);
    bsib_write(td, dstd, &sib);

    dstd->bn_key_cnt += srcd->bn_key_cnt;
    bnode_write(dstd);
    
    // kill right node
    return_error(bnode_int_del(td, pard, key_ndx, key_ndx + 1));
    return_error(sys_free(srcd->bn_off));

    return 0;
}

int
bnode_right_leaf_merge(btree_desc_t *td, 
		       bnode_desc_t *dstd, bnode_desc_t *srcd, 
		       bnode_desc_t *pard, uint16_t key_ndx)
{
    assert(dstd->bn_key_cnt + 1 == td->bt_val_min);
    assert(srcd->bn_key_cnt == td->bt_val_min);
    assert(dstd->bn_isleaf);
    assert(srcd->bn_isleaf);
    
    return bnode_leaf_merge(td, dstd, srcd, pard, key_ndx);
}


int
bnode_right_leaf_borrow(btree_desc_t *td, 
			bnode_desc_t *dstd, bnode_desc_t *srcd, 
			bnode_desc_t *pard, uint16_t key_ndx)
{
    BKEY_T(td, k); BKEY_T(td, k1);
    BVAL_T(td, v);
    bkey_read_one(td, srcd, 0, k);
    bval_read_one(td, srcd, 0, v);

    return_error(bnode_leaf_del(td, srcd, 0));
    // XXX could save by doing bx_write_one
    return_error(bnode_leaf_ins(td, dstd, dstd->bn_key_cnt, k, v));
    
    // raise new least key from src
    bkey_read_one(td, srcd, 0, k1);
    bkey_write_one(td, pard, key_ndx, k1);
    return 0;
}

int
bnode_left_leaf_merge(btree_desc_t *td, 
		      bnode_desc_t *dstd, bnode_desc_t *srcd, 
		      bnode_desc_t *pard, uint16_t key_ndx)
{
    assert(srcd->bn_key_cnt + 1 == td->bt_val_min);
    assert(dstd->bn_key_cnt == td->bt_val_min);
    assert(dstd->bn_isleaf);
    assert(srcd->bn_isleaf);

    return bnode_leaf_merge(td, dstd, srcd, pard, key_ndx - 1);
}

int
bnode_left_leaf_borrow(btree_desc_t *td, 
		       bnode_desc_t *dstd, bnode_desc_t *srcd, 
		       bnode_desc_t *pard, uint16_t key_ndx)
{
    uint16_t i = srcd->bn_key_cnt - 1;
    BKEY_T(td, k);
    BVAL_T(td, v);
    bkey_read_one(td, srcd, i, k);
    bval_read_one(td, srcd, i, v);

    return_error(bnode_leaf_del(td, srcd, i));
    return_error(bnode_leaf_ins(td, dstd, 0, k, v));

    // raise new least key from dst
    bkey_write_one(td, pard, key_ndx - 1, k);
    return 0;
}

int
bnode_leaf_split(btree_desc_t *td, bnode_desc_t *rd, bnode_desc_t *nd, 
		 bkey_t *key, bval_t *val, 
		 bkey_t *split_key, offset_t *split_off)
{
    assert(rd->bn_key_cnt == td->bt_val_max);
    assert(nd->bn_key_cnt == 0);
    assert(rd->bn_isleaf);
    assert(nd->bn_isleaf);

    char keys0[td->bt_key_tot], keys1[td->bt_key_tot];
    char vals0[td->bt_val_tot], vals1[td->bt_val_tot];
    
    bkeys_read(td, rd, keys0);
    bvals_read(td, rd, vals0);

    char f = 0;
    uint32_t i = bkey_index(td, rd, key, keys0, &f);
    if (f)
	return -E_INVAL;
    
    uint16_t keys0_cnt = rd->bn_key_cnt, keys1_cnt;
    uint16_t vals0_cnt = rd->bn_key_cnt, vals1_cnt;
    return_error(bkey_split(td, i, key, 
			    keys0, &keys0_cnt, 
			    keys1, &keys1_cnt));
    return_error(bval_split(td, i, val, 
			    vals0, &vals0_cnt,
			    vals1, &vals1_cnt));

    assert(keys0_cnt == vals0_cnt);
    assert(keys1_cnt == vals1_cnt);

    rd->bn_key_cnt = keys0_cnt;
    nd->bn_key_cnt = keys1_cnt;

    bkeys_write(td, rd, keys0);
    bvals_write(td, rd, vals0);

    // set sib
    offset_t sib;
    bsib_read(td, rd, &sib);
    bsib_write(td, rd, &nd->bn_off);
    bsib_write(td, nd, &sib);

    bkeys_write(td, nd, keys1);
    bvals_write(td, nd, vals1);
    
    bnode_write(nd);
    bnode_write(rd);

    bkey_copy(td, split_key, bkeyi(td, keys1, 0));
    *split_off = nd->bn_off;

    return 0;
}

void
bnode_print(btree_desc_t *td, bnode_desc_t *nd)
{
    char keys[td->bt_key_tot];
    bkeys_read(td, nd, keys);

    wprintf("%s (%lx) [.", nd->bn_isleaf ? "L" : "I", nd->bn_off);
    
    uint32_t i = 0;
    for (; i < nd->bn_key_cnt; i++) {
	bkey_t *k = bkeyi(td, keys, i);
	wprintf(" ");
	bkey_print(td, k);
	wprintf(" .");
    }
    wprintf("]\n");
}

void
bnode_verify_keys(btree_desc_t *td, bnode_desc_t *nd,
		  bkey_t *lo, bkey_t *hi)
{
    char keys[td->bt_key_tot];
    bkeys_read(td, nd, keys);
    
    if (!nd->bn_isleaf) {
	BKEY_T(td, lo1); BKEY_T(td, hi1);
	bkey_copy(td, lo1, lo);
	bkey_copy(td, hi1, bkeyi(td, keys, 0));
	for (uint16_t i = 0; i < nd->bn_key_cnt + 1; i++) {
	    bnode_desc_t cd;
	    assert_error(bnode_child_read(td, nd, i, &cd));
	    bnode_verify_keys(td, &cd, lo1, hi1);
	    
	    bkey_copy(td, lo1, hi1);
	    if (i == nd->bn_key_cnt - 1)
		bkey_copy(td, hi1, hi);
	    else
		bkey_copy(td, hi1, bkeyi(td, keys, i + 1));
	}
    }
    
    // ascending order
    for (uint16_t i = 0; i < nd->bn_key_cnt - 1; i++) {
	int r = bkey_cmp(td, bkeyi(td, keys, i), bkeyi(td, keys, i + 1));
	if (r < 0)
	    continue;

	wprintf("bnode_verify_keys: failure at node %"OFF_F":\n", nd->bn_off);
	wprintf(" keys not in ascending order --"
		" attempting to print tree...\n");
	btree_print(td);
	assert(0);
    }
    
    // check range [lo, hi)
    int r = bkey_cmp(td, bkeyi(td, keys, 0), lo);
    if (r < 0) {
	wprintf("bnode_verify_keys: failure at node %"OFF_F":\n", nd->bn_off);
	wprintf(" first key < lo attempting to print tree...\n");
	btree_print(td);
	assert(0);
	
    }

    r = bkey_cmp(td, bkeyi(td, keys, nd->bn_key_cnt - 1), hi);
    if (r >= 0) {
	wprintf("bnode_verify_keys: failure at node %"OFF_F":\n", nd->bn_off);
	wprintf(" last key > hi -- attempting to print tree...\n");
	btree_print(td);
	assert(0);
    }
}
