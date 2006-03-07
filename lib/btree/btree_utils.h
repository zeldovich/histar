#ifndef BT_UTILS_H_
#define BT_UTILS_H_

#include <machine/types.h>
#include <kern/lib.h>
#include <inc/queue.h>

typedef uint64_t offset_t ;

#define BTREE_FLAG_LEAF     1  

#define BTB_SET_FLAG(block, flag)    (block).flags |= ((flag) << 4)
#define BTB_GET_FLAG(block, flag)    (((block).flags >> 4) & (flag))

#define BTREE_IS_LEAF(node) (BTB_GET_FLAG((node)->block, BTREE_FLAG_LEAF) == 1)
#define BTREE_SET_LEAF(node) BTB_SET_FLAG((node)->block, BTREE_FLAG_LEAF)

#define BTB_SET_FLAG(block, flag)    (block).flags |= ((flag) << 4)
#define BTB_GET_FLAG(block, flag)    (((block).flags >> 4) & (flag))

#define BTREE_NODE_SIZE(order, key_size) \
		(sizeof(struct btree_node) + \
		sizeof(offset_t) * order + \
		sizeof(uint64_t) * (order - 1) * (key_size))

#define BTREE_LEAF_ORDER(leaf) \
	(leaf->tree->order / leaf->tree->s_value)

struct btree_node ;
struct btree ;
LIST_HEAD(node_list, btree_node);

void btree_lock(struct btree *tree)  ;
void btree_unlock(struct btree *tree) ;

#define BTREE_OP_ATTR	static __inline __attribute__((always_inline))

BTREE_OP_ATTR const offset_t *
btree_key(const offset_t *keys, const int i, int s_key)
{
    return &keys[i * s_key] ;
}

BTREE_OP_ATTR int64_t
btree_keycmp(const offset_t *key1, const offset_t *key2, int s_key)
{
    int i = 0 ; 
    int64_t r = 0 ;
    for (; r == 0 && i < s_key ; i++)
	r = key1[i] - key2[i] ;	
    return r ;
}

BTREE_OP_ATTR void
btree_keycpy(const offset_t *dst, const offset_t *src, int s_key)
{
    memcpy((offset_t *)dst, src, s_key * sizeof(offset_t)) ;
}

BTREE_OP_ATTR void
btree_keymove(const offset_t *dst, const offset_t *src, int s_key)
{
    memmove((offset_t *)dst, src, s_key * sizeof(offset_t)) ;
}

BTREE_OP_ATTR void
btree_keyset(const offset_t *dst, offset_t val, int s_key)
{
    offset_t *d = (offset_t *) dst;
    
    if (s_key) {
	do
	    *d++ = val;
	while (--s_key != 0);
    }
}

BTREE_OP_ATTR const offset_t *
btree_value(const offset_t *vals, const int i, int s_val)
{
    return &vals[i * s_val] ;
}

BTREE_OP_ATTR void
btree_valcpy(const offset_t *dst, const offset_t *src, int s_val)
{
    memcpy((offset_t *)dst, src, s_val * sizeof(offset_t)) ;
}

BTREE_OP_ATTR void
btree_valmove(const offset_t *dst, const offset_t *src, int s_val)
{
    memmove((offset_t *)dst, src, s_val * sizeof(offset_t)) ;
}

BTREE_OP_ATTR void
btree_valset(const offset_t *dst, offset_t val, int s_val)
{
    offset_t *d = (offset_t *) dst;

    if (s_val) {
	do
	    *d++ = val;
	while (--s_val != 0);
    }
}

#endif /*BT_UTILS_H_*/
