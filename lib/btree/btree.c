#include <lib/btree/btree.h>
#include <lib/btree/btree_header.h>
#include <lib/btree/btree_node.h>
#include <inc/string.h>
#include <inc/assert.h> 

void
btree_init(struct btree * t, char order, char key_size, 
		   char value_size, struct btree_manager * mm)
{
	memset(t, 0, sizeof(struct btree));
	
	t->order = order;
	
	t->min_leaf = ((t->order / value_size) / 2);
	
	t->min_intrn  = ((t->order + 1) / 2) - 1;
	t->s_key = key_size ;
	t->s_value = value_size ;

	t->op = btree_op_none ;
	t->threads = 0 ;

	t->magic = BTREE_MAGIC ;
	
	btree_manager_is(t, mm) ;
}

static void
__btree_erase(struct btree *t, offset_t root)
{
	struct btree_node *n = btree_read_node(t, root) ;
	
	if (!BTREE_IS_LEAF(n)) {
		for (int i = 0 ; i <= n->keyCount ; i++)
			__btree_erase(t, n->children[i]) ;	
	}
	btree_erase_node(n) ;
}

void
btree_erase(struct btree *t)
{
	if (t->root)
		__btree_erase(t, t->root) ;	
	t->size = 0 ;
	t->left_leaf = 0 ;
	t->root = 0 ;
}

uint64_t
btree_size(struct btree *tree)
{
	if (tree == NULL)
		return 0;

	tree->size = bt_tree_size(tree);

	return tree->size;
}

char
btree_is_empty(struct btree *tree)
{
	if (tree == NULL)
		return 1;

	return (btree_size(tree) == 0);
}

void
btree_manager_is(struct btree *t, struct btree_manager *mm)
{
	if (mm)
		memcpy(&t->manager, mm, sizeof(struct btree_manager)) ;	
}

///////////////////////////////////
// concurrency
///////////////////////////////////

void
btree_set_op(struct btree *tree, btree_op op)
{
	lock_acquire(&tree->lock) ;

	if (tree->op) {
		if (tree->op == btree_op_search &&
			op == btree_op_search)
			tree->threads++ ;	
		else
			panic("btree_set_op: setting %d while %d, %d times",
				  op, tree->op, tree->threads) ;
	}
	else {
		tree->op = op ;
		tree->threads++ ;	
	}
}

void
btree_unset_op(struct btree *tree, btree_op op)
{
	lock_release(&tree->lock) ;
	
	if (tree->op == op && tree->threads) {
		tree->threads-- ;
		if (tree->threads == 0)
			tree->op = btree_op_none ;	
	}
	else 
		panic("btree_unset_op: unsetting %d while %d, %d times",
			  op, tree->op, tree->threads) ;
}
