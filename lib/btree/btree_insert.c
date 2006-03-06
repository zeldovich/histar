/**
 * @file btree_insert.c Insertion functions
 * 
 * $Id: btree_insert.c,v 1.12 2002/04/07 18:29:40 chipx86 Exp $
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
#include <kern/lib.h>
#include <inc/error.h>

static char
__splitNode(struct btree *tree, 
			struct btree_node *rootNode, 
			uint64_t *key,
			offset_t *filePos, 
			char *split)
{
	struct btree_node *tempNode;
	uint64_t   temp1[tree->s_key];
	offset_t   offset1 = 0, offset2;
	int        i, j, div;

	assert(!BTREE_IS_LEAF(rootNode)) ;


	for (i = 0;
		i < (tree->order - 1) && 
		btree_keycmp(key, btree_key(rootNode->keys, i, tree->s_key), tree->s_key) > 0 ;
		i++)
		;

	if (i < (tree->order - 1) && 
		btree_keycmp(key, btree_key(rootNode->keys, i, tree->s_key), tree->s_key) == 0 )
	{
		*split = 0;
		return 0;
	}

	*split = 1;
	
	if (i < (tree->order - 1))
	{
        // Save order-2 key
        btree_keycpy(temp1, btree_key(rootNode->keys, (tree->order - 2), tree->s_key), tree->s_key) ;
        // Shift keys i through order-3 into i+1 through order-2
        btree_keymove(btree_key(rootNode->keys, i + 1, tree->s_key), 
                      btree_key(rootNode->keys, i, tree->s_key), 
                      tree->s_key * (tree->order - 1 - i)) ;
        // Insert new key
        btree_keycpy(btree_key(rootNode->keys, i, tree->s_key) , key, tree->s_key) ;

        j = i + 1 ;
                
		offset1 = rootNode->children[j];
        // Insert new pointer
		rootNode->children[j] = *filePos;
		
        // Shift pointers j through order-1 into j+1 through order
		for (j++; j <= (tree->order - 1); j++)
		{
			offset2 = rootNode->children[j];
			rootNode->children[j] = offset1;
			offset1 = offset2;
		}
        // Saved last pointer in offset1
	}
	else
	{
		btree_keycpy(temp1, key, tree->s_key) ;

		offset1 = *filePos;
	}

	div = (int)(tree->order / 2);

	btree_keycpy(key, btree_key(rootNode->keys, div, tree->s_key), tree->s_key) ;
	
	tempNode           = btree_new_node(tree);
	tempNode->keyCount = tree->order - 1 - div;

	i = div + 1;

    // Copy right half of keys into new right sibling
    btree_keycpy(btree_key(tempNode->keys, 0, tree->s_key), 
                 btree_key(rootNode->keys, i, tree->s_key), 
                 tree->s_key * (tempNode->keyCount - 1)) ;
    
    btree_keyset(btree_key(rootNode->keys, i, tree->s_key), 
                 0, 
                 tree->s_key * (tempNode->keyCount - 1)) ;	
    

    // Copy right half of pointers into new right sibling    
    for (j = 0; j < tempNode->keyCount ; j++, i++)
	{
        tempNode->children[j] = rootNode->children[i];
		rootNode->children[i] = 0;
	}

    // Tack on the ends saved earlier    
	btree_keycpy(btree_key(tempNode->keys, tempNode->keyCount - 1, tree->s_key), temp1, tree->s_key) ;
    tempNode->children[j] = offset1;
	
	*filePos = btree_write_node(tempNode);

	rootNode->keyCount = div;

	btree_keyset(btree_key(rootNode->keys, (int)rootNode->keyCount, tree->s_key), 0, tree->s_key) ;

	btree_write_node(rootNode);

	btree_destroy_node(tempNode);

	return 1;
}

static char
__splitLeaf(struct btree *tree, 
			struct btree_node *rootNode, 
			uint64_t *key,
			offset_t *filePos, 
			char *split)
{
	struct btree_node *tempNode;
	uint64_t   temp1[tree->s_key] ;
	offset_t   offset1[tree->s_value];
	int        i, j, div;

	assert(BTREE_IS_LEAF(rootNode)) ;

	for (i = 0;
		i < (BTREE_LEAF_ORDER(rootNode) - 1) && 
		btree_keycmp(key, btree_key(rootNode->keys, i, tree->s_key), tree->s_key) > 0 ;
		i++)
		;

	if (i < (BTREE_LEAF_ORDER(rootNode) - 1) && 
		btree_keycmp(key, btree_key(rootNode->keys, i, tree->s_key), tree->s_key) == 0 )
	{
		*split = 0;
		btree_valcpy(btree_value(rootNode->children, i, tree->s_value), filePos, tree->s_value) ;
		return 1;
	}

	*split = 1;
	
	if (i < (BTREE_LEAF_ORDER(rootNode) - 1))
	{
        // Save order-2 key
        btree_keycpy(temp1, btree_key(rootNode->keys, (BTREE_LEAF_ORDER(rootNode) - 2), tree->s_key), tree->s_key) ;
        // Shift keys i through order-3 into i+1 through order-2
        btree_keymove(btree_key(rootNode->keys, i + 1, tree->s_key), 
                      btree_key(rootNode->keys, i, tree->s_key), 
                      tree->s_key * (BTREE_LEAF_ORDER(rootNode) - 1 - i)) ;
        // Insert new key
        btree_keycpy(btree_key(rootNode->keys, i, tree->s_key) , key, tree->s_key) ;

        j = i ;

        // Save order-1 value
        btree_valcpy(offset1, 
                     btree_value(rootNode->children, 
                                 (BTREE_LEAF_ORDER(rootNode) - 1), 
                                 tree->s_value),
                     tree->s_value) ;   
        // Shift values j through order-2 into j+1 order-1
        btree_valmove(btree_value(rootNode->children, j + 1, tree->s_value),
                      btree_value(rootNode->children, j, tree->s_value),
                      tree->s_value * (BTREE_LEAF_ORDER(rootNode) - 1 - j)) ;
        // Insert new value
        btree_valcpy(btree_value(rootNode->children, j, tree->s_value),
                     filePos,
                     tree->s_value) ;
	}
	else
	{
		btree_keycpy(temp1, key, tree->s_key) ;

		btree_valcpy(offset1, 
					 btree_value(rootNode->children, 
					 			 BTREE_LEAF_ORDER(rootNode) - 1, 
					 			 tree->s_value), 
		 			 tree->s_value) ;
		btree_valcpy(btree_value(rootNode->children, 
					 			 BTREE_LEAF_ORDER(rootNode) - 1, 
					 			 tree->s_value), 
		 			 filePos,
		 			 tree->s_value) ;
			
		
	}

	div = (int)((BTREE_LEAF_ORDER(rootNode) + 1) / 2) - 1;

	btree_keycpy(key, btree_key(rootNode->keys, div, tree->s_key), tree->s_key) ;
	
	tempNode           = btree_new_node(tree);
	tempNode->keyCount = BTREE_LEAF_ORDER(rootNode) - 1 - div;

	BTREE_SET_LEAF(tempNode);

	i = div + 1;

    // Copy right half of keys into new right sibling
    btree_keycpy(btree_key(tempNode->keys, 0, tree->s_key), 
                 btree_key(rootNode->keys, i, tree->s_key), 
                 tree->s_key * (tempNode->keyCount - 1)) ;

    btree_keyset(btree_key(rootNode->keys, i, tree->s_key), 
                 0, 
                 tree->s_key * (tempNode->keyCount - 1)) ;

    // Copy right half of values into new right sibling
    btree_valcpy(btree_value(tempNode->children, 0, tree->s_value),
                 btree_value(rootNode->children, i, tree->s_value),
                 tree->s_value * (tempNode->keyCount)) ;
                 
    btree_valset(btree_value(rootNode->children, i, tree->s_value), 
                 0,
                 tree->s_value * (tempNode->keyCount)) ;

    // Tack on the ends saved earlier   
    btree_keycpy(btree_key(tempNode->keys, tempNode->keyCount - 1, tree->s_key), temp1, tree->s_key) ;
	btree_valcpy(btree_value(tempNode->children, tempNode->keyCount, tree->s_value), 
				 offset1,
				 tree->s_value) ;
	
	btree_valset(filePos, 0, tree->s_value) ;
	*filePos = btree_write_node(tempNode);

	rootNode->keyCount = div + 1;
	btree_valcpy(btree_value(rootNode->children, 
							(int)rootNode->keyCount, 
							tree->s_value),
				 filePos,
				 tree->s_value) ;
					 

	btree_write_node(rootNode);

	btree_destroy_node(tempNode);

	return 1;
}

static char
__addKey(struct btree *tree, 
		 struct btree_node *rootNode, 
		 uint64_t *key, 
		 offset_t *filePos,
		 char *split)
{
	offset_t  offset1, offset2;
	int       i, j;

	assert(!BTREE_IS_LEAF(rootNode)) ;

	*split = 0;

	for (i = 0;
		 i < rootNode->keyCount && 
		 btree_keycmp(key, btree_key(rootNode->keys, i, tree->s_key), tree->s_key) > 0 ;
		 i++)
		;

	
	if (i < rootNode->keyCount && 
		btree_keycmp(key, btree_key(rootNode->keys, i, tree->s_key), tree->s_key) == 0 )
	{
		return 0;
	}
	

	rootNode->keyCount++;

    // Shift keys i through keyCount-2 into i+1 through keyCount-1
    btree_keymove(btree_key(rootNode->keys, i+1, tree->s_key),
              btree_key(rootNode->keys, i,   tree->s_key),
              tree->s_key * (rootNode->keyCount - i - 1));

    btree_keycpy(btree_key(rootNode->keys, i, tree->s_key), key, tree->s_key) ;
    
    j = i + 1;
    
    // Shift child pointers j through keycount - 1 into 
    // j through keycount  
	offset1 = rootNode->children[j];
	rootNode->children[j] = *filePos;
	
	for (j++; j <= rootNode->keyCount; j++)
	{
		offset2 = rootNode->children[j];
		rootNode->children[j] = offset1;
		offset1 = offset2;
	}
	

	btree_write_node(rootNode);

	return 1;
}

static char
__addKeyToLeaf(struct btree *tree, 
		 struct btree_node *rootNode, 
		 uint64_t *key, 
		 offset_t *filePos,
		 char *split)
{
	int       i, j;

	assert(BTREE_IS_LEAF(rootNode)) ;

	*split = 0;

	for (i = 0;
		 i < rootNode->keyCount && 
		 btree_keycmp(key, btree_key(rootNode->keys, i, tree->s_key), tree->s_key) > 0 ;
		 i++)
		;

	
	if (i < rootNode->keyCount && 
		btree_keycmp(key, btree_key(rootNode->keys, i, tree->s_key), tree->s_key) == 0 )
	{
		btree_valcpy(btree_value(rootNode->children, i, tree->s_value), filePos, tree->s_value) ;
		return 0;
	}
	

	rootNode->keyCount++;

	// Shift keys i through keyCount-2 into i+1 through keyCount-1
	btree_keymove(btree_key(rootNode->keys, i+1, tree->s_key),
		      btree_key(rootNode->keys, i,   tree->s_key),
		      tree->s_key * (rootNode->keyCount - i - 1));

	btree_keycpy(btree_key(rootNode->keys, i, tree->s_key), key, tree->s_key) ;
	
	j = i;
	
	// Shift values j through keyCount-1 into j+1 through keyCount
	// Leafs have keyCount + 1 values.  The extra points to the
    // 'right' sibling
	btree_valmove(btree_value(rootNode->children, j+1, tree->s_value),
		      btree_value(rootNode->children, j,   tree->s_value),
		      tree->s_value * (rootNode->keyCount - j));

	btree_valcpy(btree_value(rootNode->children, j, tree->s_value),
				 filePos,
				 tree->s_value) ;

	btree_write_node(rootNode);

	return 1;
}

static char
__insertKey(struct btree *tree, 
			offset_t rootOffset, 
			uint64_t *key,
			offset_t *filePos, 
			char *split)
{
	char success = 0;
	struct btree_node *rootNode;

	rootNode = btree_read_node(tree, rootOffset);

	if (BTREE_IS_LEAF(rootNode))
	{
		if (rootNode->keyCount < (BTREE_LEAF_ORDER(rootNode) - 1))
			success = __addKeyToLeaf(tree, rootNode, key, filePos, split);
		else
			success = __splitLeaf(tree, rootNode, key, filePos, split);

		btree_destroy_node(rootNode);
		return success;
	}
	else
	{
		/* Internal node. */
		int i;

		for (i = 0;
			 i < rootNode->keyCount && 
			 btree_keycmp(key, btree_key(rootNode->keys, i, tree->s_key), tree->s_key) > 0 ;
			 i++)
			;
		
		success = __insertKey(tree, rootNode->children[i], key, filePos,
							  split);
	}

	assert(!BTREE_IS_LEAF(rootNode)) ;

	if (success == 1 && *split == 1)
	{
		if (rootNode->keyCount < (tree->order - 1))
			__addKey(tree, rootNode, key, filePos, split);
		else
			__splitNode(tree, rootNode, key, filePos, split);
	}

	btree_destroy_node(rootNode);
	
	return success;
}

int
btree_insert(void *t, const uint64_t *key, offset_t *val)
{
	char  success, split;
	struct btree *tree = (struct btree *)t ;
	assert(tree->magic == BTREE_MAGIC) ;

	uint64_t k[tree->s_key] ;
	uint64_t v[tree->s_value] ;
	
	btree_keycpy(k, key, tree->s_key) ;
	if (val)
		btree_valcpy(v, val, tree->s_value) ;
	else
		btree_valset(v, 0, tree->s_value) ;
	
	if (tree == NULL || key == 0)
		return -E_INVAL;

	success = 0;
	split = 0;

	btree_lock(tree) ;
	tree->_insFilePos = v ;
	
	if (tree->root != 0)
	{
		success = __insertKey(tree, tree->root, k, tree->_insFilePos,
							  &split);

		if (success == 0)
		{
			// duplicate
			btree_unlock(tree) ;
			return -E_INVAL;;
		}
	}
	
	bt_tree_size_is(tree, tree->size + 1) ;

	if (tree->root == 0 || split == 1)
	{
		struct btree_node *node = btree_new_node(tree);

		btree_keycpy(btree_key(node->keys, 0, tree->s_key), k, tree->s_key) ;
		node->keyCount    = 1;

		if (tree->root == 0)
		{
			BTREE_SET_LEAF(node);
			btree_valcpy(btree_value(node->children, 0, tree->s_value) ,
						 tree->_insFilePos,
						 tree->s_value) ;

			btree_write_node(node);

			bt_left_leaf_is(tree, node->block.offset);
		}
		else
		{
			node->children[0] = tree->root;
			node->children[1] = *tree->_insFilePos;

			btree_write_node(node);
		}
                btree_height_is(tree, tree->height + 1) ;
		btree_root_node_is(tree, node->block.offset);
		btree_destroy_node(node);
	}

	btree_unlock(tree) ;
	return 0;	
}
