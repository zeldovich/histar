#include <btree/btree.h>
#include <btree/btree_node.h>
#include <kern/lib.h>
#include <btree/btree_impl.h>

void
btree_init_impl(struct btree * t, uint64_t id, char order, char key_size, char value_size)
{
	memset(t, 0, sizeof(struct btree));
	
	t->order = order;
	
	t->min_leaf = ((t->order / value_size) / 2);
	
	t->min_intrn  = ((t->order + 1) / 2) - 1;
	t->s_key = key_size ;
	t->s_value = value_size ;
    t->id = id ;

	t->magic = BTREE_MAGIC ;
	t->size = 0 ;
    t->height = 0 ;
        
}

uint64_t
btree_size_impl(struct btree *tree)
{
	if (tree == NULL)
		return 0;

	return tree->size;
}

char
btree_is_empty_impl(struct btree *tree)
{
	if (tree == NULL)
		return 1;

	return (btree_size_impl(tree) == 0);
}
