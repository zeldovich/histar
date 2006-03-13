#ifndef BTREE_IMPL_H_
#define BTREE_IMPL_H_

#include <btree/btree.h>
#include <btree/cache.h>
#include <inc/types.h>

int btree_insert_impl(struct btree *tree, const uint64_t * key,
		      offset_t * val);
char btree_delete_impl(struct btree *tree, const uint64_t * key);
char btree_is_empty_impl(struct btree *tree);
void btree_init_impl(struct btree *t, uint64_t id, char order, char key_size,
		     char value_size);
uint64_t btree_size_impl(struct btree *tree);

// match key exactly
int btree_search_impl(struct btree *tree, const uint64_t * key,
		      uint64_t * key_store, uint64_t * val_store);
// match the closest key less than or equal to the given key
int btree_ltet_impl(struct btree *tree, const uint64_t * key,
		    uint64_t * key_store, uint64_t * val_store);
// match the closest key greater than or equal to the given key
int btree_gtet_impl(struct btree *tree, const uint64_t * key,
		    uint64_t * key_store, uint64_t * val_store);

#endif /*BTREE_IMPL_H_ */
