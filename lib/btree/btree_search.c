/**
 * @file btree_search.c Searching functions
 * 
 * $Id: btree_search.c,v 1.4 2002/03/28 11:35:16 chipx86 Exp $
 *
 * @Copyright (C) 1999-2002 The GNUpdate Project.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA  02111-1307, USA.
 */

#include <lib/btree/btree_utils.h>
#include <lib/btree/btree_node.h>
#include <lib/btree/btree_header.h>
#include <inc/error.h>
#include <inc/assert.h>

enum {
	match_eq = 0,
    match_ltet,
    match_gtet,
} ;

static char
__search(struct btree *tree, 
		 offset_t rootOffset, 
		 const uint64_t *key, 
		 char match, 
		 struct btree_node *last_right, 
		 int div,
 		  uint64_t *key_store, 
		  offset_t *val_store)
{
	int i;
	struct btree_node *rootNode;
	char result;

	rootNode = btree_read_node(tree, rootOffset);
	assert(rootNode);
	
	for (i = 0;
		 i < rootNode->keyCount && 
		 btree_keycmp(btree_key(rootNode->keys, i, tree->s_key), key, tree->s_key) < 0 ;
		 i++)
		;

	if (BTREE_IS_LEAF(rootNode))
	{
		if (i < rootNode->keyCount && 
			btree_keycmp(btree_key(rootNode->keys, i, tree->s_key), key, tree->s_key) == 0)
		{
			//*val_store = rootNode->children[i];
			btree_valcpy(val_store,
						 btree_value(rootNode->children, i, tree->s_value),
						 tree->s_value) ;
			btree_keycpy(key_store, 
						 btree_key(rootNode->keys, i, tree->s_key), 
						 tree->s_key) ;
			btree_destroy_node(rootNode);
			return 1;
		}
		else if (match & match_ltet)
		{
			if (i > 0) {
				//*val_store = rootNode->children[i - 1] ;	
				btree_valcpy(val_store, 
							 btree_value(rootNode->children, i - 1, tree->s_value),
							 tree->s_value) ;
				btree_keycpy(key_store, 
							 btree_key(rootNode->keys, i - 1, tree->s_key),
							 tree->s_key) ;
				btree_destroy_node(rootNode);
				return 1 ;
			}
			else if (div > 0) {
				// ok ,accessing a twig node or higher
				struct btree_node *n = btree_read_node(tree, last_right->children[div - 1] );
				struct btree_node *temp ;
				
				while(!BTREE_IS_LEAF(n)) {
					temp = btree_read_node(tree, n->children[(int)n->keyCount]);
					btree_destroy_node(n) ;
					n = temp ;
				}
				
				//*val_store = n->children[n->keyCount-1] ;
				btree_valcpy(val_store, 
							 btree_value(n->children, n->keyCount - 1, tree->s_value),
							 tree->s_value) ;
				btree_keycpy(key_store,
							 btree_key(n->keys, n->keyCount-1, tree->s_key),
							 tree->s_key) ;
				btree_destroy_node(n) ;
				return 1 ;
			}
			
		}
		else if (match & match_gtet)
		{
			if (i < rootNode->keyCount) {
				//*val_store = rootNode->children[i] ;	
				btree_valcpy(val_store, 
							 btree_value(rootNode->children, i, tree->s_value),
							 tree->s_value) ;
				
				btree_keycpy(key_store,
							 btree_key(rootNode->keys, i, tree->s_key),
							 tree->s_key) ;
				btree_destroy_node(rootNode);
				return 1 ;
			}
			else if (rootNode->children[(int)rootNode->keyCount]){
				//struct btree_node *n = bt_read_node(tree, rootNode->children[(int)rootNode->keyCount]);
				const offset_t *temp1 = btree_value(rootNode->children,
													rootNode->keyCount, 
								 				   	tree->s_value) ;
				// ok, reading next pointer in node
				struct btree_node *n = btree_read_node(tree, *temp1);
				//*val_store = n->children[0] ;
				btree_valcpy(val_store, 
							 btree_value(n->children, 0, tree->s_value),
							 tree->s_value) ;
				btree_keycpy(key_store,
							 btree_key(n->keys, 0, tree->s_key),
							 tree->s_key) ;
				btree_destroy_node(n) ;
				btree_destroy_node(rootNode);
				return 1 ;
			}
		}
	
		btree_destroy_node(rootNode);
		
		return 0;
	}
	
	// accesses to children ok, can't be leafs
	assert(rootNode->children);

	if (i > 0)
		result = __search(tree, rootNode->children[i], key, match, rootNode, i, key_store, val_store);
	else
		result = __search(tree, rootNode->children[i], key, match, last_right, div, key_store, val_store);

	btree_destroy_node(rootNode);

	return result;
}

static int
__btree_search(struct btree *tree, 
		  const uint64_t *key, 
		  char match, 
		  uint64_t *key_store,
		  uint64_t *val_store)
{
	char found;
	
	//uint64_t val_store ;
	
	if (tree == NULL || key == 0)
		return -E_NOT_FOUND;

	found   = 0;

	/* Read in the tree data. */
	tree->root     = bt_root_node(tree);
	tree->left_leaf = bt_left_leaf(tree);

	if (tree->root == 0)
		return -E_NOT_FOUND ;

	if (btree_is_empty(tree) == 1)
		return -E_NOT_FOUND;

	btree_set_op(tree, btree_op_search) ;
	found = __search(tree, 
					 tree->root, 
					 key, 
					 match, 
					 0, 
					 0, 
					 key_store, 
					 val_store);
	btree_unset_op(tree, btree_op_search) ;
	
	
	if (found != 0)
		return 0 ;

	return -E_NOT_FOUND ;
}

int
btree_search(struct btree *tree, const uint64_t *key, 
			 uint64_t *key_store, uint64_t *val_store)
{
	return __btree_search(tree, key, match_eq, key_store, val_store) ;	
}

int 
btree_ltet(struct btree *tree, const uint64_t *key, 
		   uint64_t *key_store, uint64_t *val_store)
{
	return __btree_search(tree, key, match_ltet, key_store, val_store) ;	
}
int 
btree_gtet(struct btree *tree, const uint64_t *key, 
		   uint64_t *key_store, uint64_t *val_store)
{
	return __btree_search(tree, key, match_gtet, key_store, val_store) ;	
}
