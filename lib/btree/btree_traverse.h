#ifndef BTREE_TRAVERSE_H_
#define BTREE_TRAVERSE_H_

struct btree_traversal
{
	struct btree		*tree;       
	struct btree_node 	*node;       
	
	uint16_t pos;        		 
};

int 	 btree_init_traversal(struct btree *tree, struct btree_traversal *trav) ;
// calls 'process' on each value in all leafs
void 	 btree_traverse(struct btree *tree, void (*process)(offset_t filePos)) ;
offset_t btree_first_offset(struct btree_traversal *trav);
offset_t btree_next_offset(struct btree_traversal *trav);


#endif /*BTREE_TRAVERSE_H_*/
