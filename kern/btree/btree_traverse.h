#ifndef BTREE_TRAVERSE_H_
#define BTREE_TRAVERSE_H_

#include <btree/btree_utils.h>
#include <inc/types.h>
#include <btree/btree.h>

int 	btree_init_traversal_impl(struct btree *tree, struct btree_traversal *trav) ;
char 	btree_first_offset(struct btree_traversal *trav);

#endif /*BTREE_TRAVERSE_H_*/
