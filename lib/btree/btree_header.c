#include <lib/btree/btree.h>
#include <lib/btree/btree_header.h>

void 
btree_root_node_is(struct btree *tree, offset_t offset)
{
	tree->root = offset ;
}

void
bt_tree_size_is(struct btree *tree, unsigned long size)
{
	tree->size = size ;
}

void
btree_height_is(struct btree *tree, uint64_t h)
{
        tree->height = h ;       
}

offset_t
bt_root_node(struct btree *tree)
{
	return tree->root ;	
}

offset_t
bt_left_leaf(struct btree * tree)
{
	return tree->left_leaf ;	
}

void 
bt_left_leaf_is(struct btree * tree, offset_t offset)
{
	tree->left_leaf = offset;
}

unsigned long
bt_tree_size(struct btree * tree)
{
	return tree->size ;	
}
