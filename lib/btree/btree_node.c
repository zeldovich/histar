#include <lib/btree/btree.h>
#include <lib/btree/btree_node.h>
#include <kern/lib.h>

struct btree_node *
btree_new_node(struct btree *tree)
{
	struct btree_node *node ; 
	if (tree->manager.alloc(tree, &node, tree->manager.arg) < 0)
		panic("btree_new_node: unable to alloc node") ;

	
	memset(node->children, 0, sizeof(offset_t) * tree->order)	 ;
	memset((void *)node->keys, 0, 
		   sizeof(uint64_t) * (tree->order - 1) * (tree->s_key)) ;
	
	return node;
}

void
btree_destroy_node(struct btree_node * node)
{
	if (node == NULL)
		return ;
	
	struct btree *tree = node->tree ;
	
	if (tree && tree->manager.unpin_node)
		tree->manager.unpin_node(tree->manager.arg, node->block.offset) ;
	
}

struct btree_node *
btree_read_node(struct btree *tree, offset_t offset)
{
	struct btree_node *n ;
	int r = 0 ;

	if (tree->manager.node && offset) {
		if ((r = tree->manager.node(tree, offset, &n, tree->manager.arg)) == 0)
			return n ;
	}
	
	panic("btree_read_node: unable to read node %ld: %s\n", offset, e2s(r)) ;
	
	return NULL ;
}

offset_t
btree_write_node(struct btree_node *node)
{
	if (node == NULL)
		return 0;
	if (node->tree == NULL)
		return 0;

	// XXX: check dirty bit

	struct btree *tree = node->tree ;
	
	assert(tree) ;
	
	if (tree->manager.write(node, tree->manager.arg) == 0)
		return node->block.offset;
		
	panic("btree_write_node: unable to write node %ld", 
		  node->block.offset) ;
	return 0 ;
}

void
btree_erase_node(struct btree_node *node)
{
	int r ;
    struct btree *tree = node->tree ;
	
	if (tree->manager.free)
		if ((r = tree->manager.free(tree->manager.arg, node->block.offset)) < 0)
            panic("btree_erase_node: unable to erase node %ld: %s\n", node->block.offset, e2s(r)) ;
}
