#ifndef _BTREE_NODE_H_
#define _BTREE_NODE_H_

#include <lib/btree/btree.h>

struct btree_node *btree_new_node(struct btree *tree);
struct btree_node *btree_read_node(struct btree * tree, offset_t offset) ;

offset_t btree_write_node(struct btree_node *node);

void btree_erase_node(struct btree_node *node);
void btree_unpin_node(struct btree *tree, struct btree_node *node) ;

void btree_destroy_node(struct btree_node * node);

void btree_deserialize_node(struct btree_node *n, uint8_t *buf) ;
void btree_serialize_node(uint8_t *buf, struct btree_node *n) ;

#endif /* _BTREE_NODE_H_ */
