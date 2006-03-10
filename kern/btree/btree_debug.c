#include <btree/btree_debug.h>
#include <btree/btree_node.h>
#include <kern/lib.h>

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
btree_sanity_check_impl(void *tree)
{
    struct btree *btree = (struct btree *) tree ;

	uint64_t count1 = 0 ;
	if (btree->root) 
		btree_leaf_count1(btree, btree->root, &count1) ;

	uint64_t count2 = btree_leaf_count2(btree) ;
	
	if (count1 != count2) 
		panic("btree_sanity_check: count mismatch: %ld %ld\n", count1, count2) ;
	
	uint64_t size1 = btree->size ;
	uint64_t size2 = btree_size_calc(btree) ;
	
	if (size1 != size2)
		panic("btree_sanity_check: size mismatch: %ld %ld\n", size1, size2) ;
	
	if (btree->root) 
		btree_integrity_check(btree, btree->root) ;
}

static void
__btree_pretty_print(struct btree *tree, offset_t rootOffset, int i)
{
	int j;
	struct btree_node *rootNode;

	if (rootOffset == 0) {
		cprintf("[ empty ]\n") ;
		return ;
	}
	
	rootNode = btree_read_node(tree, rootOffset);

	for (j = i; j > 0; j--)
		cprintf("    ");

	cprintf("[.");

	for (j = 0; j < rootNode->keyCount; j++) {
		const offset_t *off = btree_key(rootNode->keys, j, tree->s_key) ;
		//printf(" %ld .", *off);
		cprintf(" %ld", off[0]);
		int k = 1 ;
		for (; k < tree->s_key ; k++) {
			cprintf("|%ld", off[k]) ; 	
		}
		cprintf(" .") ;
	
	}
		//printf(" %ld .", rootNode->keys[j]);
	
	if (BTREE_IS_LEAF(rootNode))
		for (j = BTREE_LEAF_ORDER(rootNode) - rootNode->keyCount; j > 1; j--)
			cprintf(" _____ .");
	else
		for (j = tree->order - rootNode->keyCount; j > 1; j--)
			cprintf(" _____ .");
	
	cprintf("] - %ld\n", rootOffset);

	if (BTREE_IS_LEAF(rootNode))
	{
		btree_destroy_node(rootNode);
		//btree_unpin_node(tree, rootNode);
		return;
	}
	
	for (j = 0; j <= rootNode->keyCount; j++)
		__btree_pretty_print(tree, rootNode->children[j], i + 1);


	btree_destroy_node(rootNode);
	//btree_unpin_node(tree, rootNode);
}

void 
btree_pretty_print_impl(void *tree)
{
    struct btree *btree = (struct btree *)tree ;
	__btree_pretty_print(btree, btree->root, 0) ;
}
