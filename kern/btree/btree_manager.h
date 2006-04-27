#ifndef BTREE_MANAGER_H_
#define BTREE_MANAGER_H_

#include <btree/btree.h>
#include <btree/btree_traverse.h>
#include <inc/types.h>

#define BTREE_OBJMAP    0
#define BTREE_IOBJ      1
#define BTREE_FCHUNK    2
#define BTREE_FOFFSET   3
#define BTREE_COUNT     4

void btree_manager_init(void);
void btree_manager_reset(void);

void btree_manager_serialize(void *buf);
void btree_manager_deserialize(void *buf);

int  btree_alloc_node(uint64_t id, uint8_t ** mem, uint64_t * off)
    __attribute__ ((warn_unused_result));
int  btree_close_node(uint64_t id, uint64_t off)
    __attribute__ ((warn_unused_result));
int  btree_open_node(uint64_t id, uint64_t off, uint8_t ** mem)
    __attribute__ ((warn_unused_result));
int  btree_save_node(uint64_t id, struct btree_node *n)
    __attribute__ ((warn_unused_result));
int  btree_free_node(uint64_t id, uint64_t off)
    __attribute__ ((warn_unused_result));
int btree_refs_node(uint64_t id, uint64_t off)
    __attribute__ ((warn_unused_result));

struct cache *btree_cache(uint64_t id);

void btree_lock(uint64_t id);
void btree_unlock(uint64_t id);

void btree_lock_all(void);
void btree_unlock_all(void);



#endif /*BTREE_MANAGER_H_ */
