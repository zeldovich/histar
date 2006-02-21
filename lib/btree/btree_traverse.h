#ifndef BTREE_TRAVERSE_H_
#define BTREE_TRAVERSE_H_
#include <lib/btree/btree_utils.h>
#include <inc/types.h>

struct btree_traversal
{
	struct btree		*tree;       
	struct btree_node 	*node;       
	
	uint16_t pos;
	
	const uint64_t	*key ;
	const offset_t	*val ;
};

int 	btree_init_traversal(struct btree *tree, struct btree_traversal *trav) ;
//void 	btree_traverse(struct btree *tree, void (*process)(offset_t filePos)) ;
char 	btree_first_offset(struct btree_traversal *trav);
char 	btree_next_entry(struct btree_traversal *trav);


#endif /*BTREE_TRAVERSE_H_*/
