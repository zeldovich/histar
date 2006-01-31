#include <lib/btree/btree.h>
#include <lib/btree/btree_header.h>
#include <inc/string.h>

void
btree_init(struct btree * t, char order, char key_size, struct btree_manager * mm)
{
	memset(t, 0, sizeof(struct btree));
	
	t->order = order;
	
	t->min_leaf = (t->order / 2);
	t->min_intrn  = ((t->order + 1) / 2) - 1;
	t->s_key = key_size ;
	
	if (mm)
		memcpy(&t->manager, mm, sizeof(struct btree_manager)) ;
	
}

void
btree_release_nodes(struct btree *tree)
{
	if (tree->manager.unpin)
		tree->manager.unpin(tree->manager.arg) ;	
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

const offset_t *
btree_key(const offset_t *keys, const int i, uint8_t s_key)
{
	return &keys[i * s_key] ;
}


int64_t
btree_keycmp(const offset_t *key1, const offset_t *key2, uint8_t s_key)
{
	int i = 0, r = 0 ;
	for (; r == 0 && i < s_key ; i++)
		r = key1[i] - key2[i] ;	
	return r ;
}

void
btree_keycpy(const offset_t *dst, const offset_t *src, uint8_t s_key)
{
	memcpy((offset_t *)dst, src, s_key * sizeof(offset_t)) ;
}

void
btree_keyset(const offset_t *dst, offset_t val, uint8_t s_key)
{
	memset((offset_t *)dst, val, s_key * sizeof(offset_t)) ;
}
