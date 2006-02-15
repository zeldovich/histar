#include <lib/btree/btree_debug.h>
#include <lib/btree/btree_node.h>
#include <inc/assert.h>

static void
btree_leaf_count1(struct btree *tree, struct btree_node *root, uint64_t *count)
{
	struct btree_node *node ;
	
	if (BTREE_IS_LEAF(root))
		*count = *count + 1 ;
	else {
		for (int i = 0 ; i <= root->keyCount ; i++) {
			node = btree_read_node(tree, root->children[i]) ;
			btree_leaf_count1(tree, node, count) ;
			btree_unpin_node(tree, node) ;
		}
	}
}

static uint64_t
btree_leaf_count2(struct btree *tree)
{
	uint64_t count = 0 ;
	offset_t next_off = tree->left_leaf ;
	struct btree_node *node ;
	
	while (next_off) {
		//cprintf("next_off %ld\n", next_off) ;
		node = btree_read_node(tree, next_off) ;
		count++ ;
		//next_off = node->children[node->keyCount] ;
		next_off = *btree_value(node->children, 
											  node->keyCount, 
											  tree->s_value);
		btree_unpin_node(tree, node) ;
	}
	
	return count ;
}

static uint64_t
btree_size_calc(struct btree *tree)
{
	uint64_t size = 0 ;
	offset_t next_off = tree->left_leaf ;
	struct btree_node *node ;
	
	while (next_off) {
		node = btree_read_node(tree, next_off) ;
		size += node->keyCount ;
		next_off = *btree_value(node->children, 
											  node->keyCount, 
											  tree->s_value);
		btree_unpin_node(tree, node) ;
	}
	
	return size ;	
}

void
btree_sanity_check(struct btree *tree)
{
	uint64_t count1 = 0 ;
	struct btree_node *root = btree_read_node(tree, tree->root) ;
	if (root)
		btree_leaf_count1(tree, root, &count1) ;
	btree_release_nodes(tree) ;

	uint64_t count2 = btree_leaf_count2(tree) ;
	btree_release_nodes(tree) ;
	
	if (count1 != count2) 
		panic("btree_sanity_check: count mismatch: %ld %ld\n", count1, count2) ;
	
	cprintf("count1 %ld count2 %ld\n", count1, count2) ;
	
	uint64_t size1 = tree->size ;
	uint64_t size2 = btree_size_calc(tree) ;
	
	if (size1 != size2)
		panic("btree_sanity_check: size mismatch: %ld %ld\n", size1, size2) ;
}
