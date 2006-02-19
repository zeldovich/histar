#include <lib/btree/btree_debug.h>
#include <lib/btree/btree_node.h>
#include <inc/assert.h>

static void
btree_leaf_count1(struct btree *tree, offset_t root, uint64_t *count)
{
	struct btree_node *root_node = btree_read_node(tree, root);
	
	if (BTREE_IS_LEAF(root_node))
		*count = *count + 1 ;
	else {
		for (int i = 0 ; i <= root_node->keyCount ; i++) {
			btree_leaf_count1(tree, root_node->children[i], count) ;
		}
	}
	btree_destroy_node(root_node) ;
}

static uint64_t
btree_leaf_count2(struct btree *tree)
{
	uint64_t count = 0 ;
	offset_t next_off = tree->left_leaf ;
	struct btree_node *node ;
	
	while (next_off) {
		node = btree_read_node(tree, next_off) ;
		count++ ;
		next_off = *btree_value(node->children, 
											  node->keyCount, 
											  tree->s_value);
		btree_destroy_node(node) ;
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
		btree_destroy_node(node) ;
	}
	
	return size ;	
}

static void
btree_integrity_check(struct btree *tree, offset_t root)
{
	struct btree_node *root_node = btree_read_node(tree, root) ;
	
	if (root_node->keyCount == 0)
		panic("node %ld: keyCount is 0", root) ;
	
	if (!BTREE_IS_LEAF(root_node)) {
		for (int i = 0 ; i <= root_node->keyCount ; i++) {
			if (root_node->children[i] == 0) 
				panic("node %ld: child is 0 when it shouldn't be", root) ;	
		}
	}
	
	btree_destroy_node(root_node) ;
}

void
btree_sanity_check(struct btree *tree)
{
	uint64_t count1 = 0 ;
	if (tree->root) 
		btree_leaf_count1(tree, tree->root, &count1) ;

	uint64_t count2 = btree_leaf_count2(tree) ;
	
	if (count1 != count2) 
		panic("btree_sanity_check: count mismatch: %ld %ld\n", count1, count2) ;
	
	uint64_t size1 = tree->size ;
	uint64_t size2 = btree_size_calc(tree) ;
	
	if (size1 != size2)
		panic("btree_sanity_check: size mismatch: %ld %ld\n", size1, size2) ;
	
	if (tree->root) 
		btree_integrity_check(tree, tree->root) ;
}

static const char *const op_string[4] = {
	"none",
	"search",
	"delete",
	"insert"	
} ;

void
btree_set_op(struct btree *tree, btree_op op)
{
	if (tree->op) {
		if (tree->op == btree_op_search &&
			op == btree_op_search)
			tree->threads++ ;	
#if 0
		else
			panic("btree_set_op: setting %d while %d, %d times",
				  op, tree->op, tree->threads) ;
#endif
	}
	else {
		tree->op = op ;
		tree->threads++ ;	
	}
}

void
btree_unset_op(struct btree *tree, btree_op op)
{
	if (tree->op == op && tree->threads) {
		tree->threads-- ;
		if (tree->threads == 0)
			tree->op = btree_op_none ;	
	}
#if 0
	else 
		panic("btree_unset_op: unsetting %d while %d, %d times",
			  op, tree->op, tree->threads) ;
#endif
}
