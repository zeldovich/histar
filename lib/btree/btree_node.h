#ifndef _BTREE_NODE_H_
#define _BTREE_NODE_H_

#include <lib/btree/btree.h>

struct btree_node *btree_new_node(struct btree *tree);
struct btree_node *btree_read_node(struct btree * tree, offset_t offset) ;

offset_t btree_write_node(struct btree_node *node);

void btree_erase_node(struct btree_node *node);
void btree_unpin_node(struct btree *tree, struct btree_node *node) ;

// XXX: deprecated, should remove from existing code
void btree_destroy_node(struct btree_node * node);

#endif /* _BTREE_NODE_H_ */
