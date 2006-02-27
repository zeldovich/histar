#ifndef _BTREE_HEADER_H_
#define _BTREE_HEADER_H_


void btree_root_node_is(struct btree *tree, offset_t offset);
void bt_left_leaf_is(struct btree * tree, offset_t offset);
void bt_tree_size_is(struct btree * tree, unsigned long size);
void btree_height_is(struct btree *tree, uint64_t h) ;

offset_t bt_root_node(struct btree *tree);
offset_t bt_left_leaf(struct btree * tree);
uint64_t bt_tree_size(struct btree * tree);

#endif /* _BTREE_HEADER_H_ */
