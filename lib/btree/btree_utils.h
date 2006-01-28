#ifndef BT_UTILS_H_
#define BT_UTILS_H_

#include <inc/types.h>

typedef uint64_t offset_t ;

#define BTREE_FLAG_LEAF     1  

#define BTB_SET_FLAG(block, flag)    (block).flags |= ((flag) << 4)
#define BTB_GET_FLAG(block, flag)    (((block).flags >> 4) & (flag))

#define BTB_IS_DIRTY(block)    (((block).dirty) == 1)
#define BTB_SET_DIRTY(block)   (block).dirty = 1
#define BTB_CLEAR_DIRTY(block) (block).dirty = 0

#define BTREE_IS_LEAF(node) (BTB_GET_FLAG((node)->block, BTREE_FLAG_LEAF) == 1)
#define BTREE_SET_LEAF(node) BTB_SET_FLAG((node)->block, BTREE_FLAG_LEAF)

#define BTB_SET_FLAG(block, flag)    (block).flags |= ((flag) << 4)
#define BTB_GET_FLAG(block, flag)    (((block).flags >> 4) & (flag))

#define BTB_IS_DIRTY(block)    (((block).dirty) == 1)
#define BTB_SET_DIRTY(block)   (block).dirty = 1
#define BTB_CLEAR_DIRTY(block) (block).dirty = 0


const offset_t * btree_key(const offset_t *keys, const int i, uint8_t s_key) ;

int64_t btree_keycmp(const offset_t *key1, const offset_t *key2, uint8_t s_key) ;
void 	btree_keycpy(const offset_t *dst, const offset_t *src, uint8_t s_key) ;
void 	btree_keyset(const offset_t *dst, offset_t val, uint8_t s_key) ;


#endif /*BT_UTILS_H_*/
