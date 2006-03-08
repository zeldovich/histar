#ifndef BTREE_TRAVERSE_H_
#define BTREE_TRAVERSE_H_

#include <lib/btree/btree_utils.h>
#include <inc/types.h>
#include <lib/btree/btree.h>

int 	btree_init_traversal_impl(struct btree *tree, struct btree_traversal *trav) ;
char 	btree_first_offset(struct btree_traversal *trav);

#endif /*BTREE_TRAVERSE_H_*/
