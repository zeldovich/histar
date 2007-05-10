#ifndef _BTREE_H_
#define _BTREE_H_

#include <inc/types.h>
#include <inc/queue.h>
#include <inc/intmacro.h>

// maximum size of a btree node -- currently cannot exceed PGSIZE
#define BTREE_BLOCK_SIZE PGSIZE

typedef uint64_t offset_t;

struct btree_node {
    struct btree *tree;

    struct {
	offset_t offset;

	uint8_t is_leaf : 1;
	uint8_t is_dirty : 1;	// used by the log
    } block;

    uint8_t keyCount;
    offset_t *children;
    const uint64_t *keys;

    TAILQ_ENTRY(btree_node) node_log_link;
    uint64_t bytesize;
};

#define BTREE_MAGIC	UINT64(0xcdef9425feed7980)
#define MAX_KEY_SIZE	2
#define MAX_VALUE_SIZE	2

struct btree {
    uint8_t order;
    uint8_t s_key;
    uint8_t s_value;
    uint8_t id;

    uint8_t min_leaf;
    uint8_t min_intrn;
    uint16_t height;

    uint64_t size;
    offset_t root;
    offset_t left_leaf;

    uint64_t magic;
};

struct btree_traversal {
    // private
    struct btree *tree;
    struct btree_node *node;
    uint16_t pos;

    // public
    const uint64_t *key;
    const offset_t *val;
};

int btree_insert(uint64_t id, const uint64_t * key, offset_t * val)
    __attribute__ ((warn_unused_result));
char btree_delete(uint64_t id, const uint64_t * key)
    __attribute__ ((warn_unused_result));

int btree_search(uint64_t id, const uint64_t * key, uint64_t * key_store,
		 uint64_t * val_store)
    __attribute__ ((warn_unused_result));
int btree_ltet(uint64_t id, const uint64_t * key, uint64_t * key_store,
	       uint64_t * val_store)
    __attribute__ ((warn_unused_result));
int btree_gtet(uint64_t id, const uint64_t * key, uint64_t * key_store,
	       uint64_t * val_store)
    __attribute__ ((warn_unused_result));

int btree_init_traversal(uint64_t id, struct btree_traversal *trav)
    __attribute__ ((warn_unused_result));
char btree_next_entry(struct btree_traversal *trav)
    __attribute__ ((warn_unused_result));

void btree_pretty_print(uint64_t id);
void btree_sanity_check(uint64_t id);

#endif /* _BTREE_H_ */
