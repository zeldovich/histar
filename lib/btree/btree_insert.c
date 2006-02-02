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
#include <inc/stdio.h>
#include <inc/error.h>

#include <inc/assert.h>

static char
__splitNode(struct btree *tree, 
			struct btree_node *rootNode, 
			uint64_t *key,
			offset_t *filePos, 
			char *split)
{
	struct btree_node *tempNode;
	uint64_t   temp1[tree->s_key], temp2[tree->s_key];
	offset_t   offset1 = 0, offset2;
	//char       tempSize1, tempSize2;
	int        i, j, div;

	assert(!BTREE_IS_LEAF(rootNode)) ;


	for (i = 0;
		i < (tree->order - 1) && 
		btree_keycmp(key, btree_key(rootNode->keys, i, tree->s_key), tree->s_key) > 0 ;
		i++)
		;

	if (i < (tree->order - 1) && 
		btree_keycmp(key, btree_key(rootNode->keys, i, tree->s_key), tree->s_key) == 0 )  //cmp
	{
		*split = 0;
		return 0;
	}

	*split = 1;
	
	if (i < (tree->order - 1))
	{
		//temp1                 = rootNode->keys[i];
		btree_keycpy(temp1, btree_key(rootNode->keys, i, tree->s_key), tree->s_key) ;
		//rootNode->keys[i]     = *key;
		btree_keycpy(btree_key(rootNode->keys, i, tree->s_key) , key, tree->s_key) ;
		j = i;

		for (i++; i < (tree->order - 1); i++)
		{
			//temp2     = rootNode->keys[i];
			btree_keycpy(temp2, btree_key(rootNode->keys, i , tree->s_key), tree->s_key) ;
			
			//rootNode->keys[i]     = temp1;
			btree_keycpy(btree_key(rootNode->keys, i, tree->s_key), temp1, tree->s_key) ;
			
			//temp1     = temp2;
			btree_keycpy(temp1, temp2, tree->s_key) ;
			//tempSize1 = tempSize2;
		}

		if (!BTREE_IS_LEAF(rootNode))
			j++;

		offset1 = rootNode->children[j];
		rootNode->children[j] = *filePos;
		
		for (j++; j <= (tree->order - 1); j++)
		{
			offset2 = rootNode->children[j];
			rootNode->children[j] = offset1;
			offset1 = offset2;
		}
	}
	else
	{
		//temp1     = *key;
		btree_keycpy(temp1, key, tree->s_key) ;
		//tempSize1 = sizeof(bt_key_t) ;

		if (BTREE_IS_LEAF(rootNode))
		{
			offset1 = rootNode->children[tree->order - 1];
			rootNode->children[tree->order - 1] = *filePos;
		}
		else
			offset1 = *filePos;
	}

	if (BTREE_IS_LEAF(rootNode))
		div = (int)((tree->order + 1) / 2) - 1;
	else
		div = (int)(tree->order / 2);

	//*key = rootNode->keys[div] ;
	btree_keycpy(key, btree_key(rootNode->keys, div, tree->s_key), tree->s_key) ;
	
	tempNode           = btree_new_node(tree);
	tempNode->keyCount = tree->order - 1 - div;

	if (BTREE_IS_LEAF(rootNode))
		BTREE_SET_LEAF(tempNode);

	i = div + 1;

	for (j = 0; j < tempNode->keyCount - 1; j++, i++)
	{
		//tempNode->keys[j]     = rootNode->keys[i];
		btree_keycpy(btree_key(tempNode->keys, j, tree->s_key), 
					 btree_key(rootNode->keys, i, tree->s_key), 
					 tree->s_key) ;
		tempNode->children[j] = rootNode->children[i];

		//rootNode->keys[i]     = 0;
		btree_keyset(btree_key(rootNode->keys, i, tree->s_key), 0, tree->s_key) ;
		rootNode->children[i] = 0;
	}

	//tempNode->keys[j]         = temp1;
	btree_keycpy(btree_key(tempNode->keys, j, tree->s_key), temp1, tree->s_key) ;
	tempNode->children[j]     = rootNode->children[i];
	rootNode->children[i]     = 0;
	tempNode->children[j + 1] = offset1;
	
	*filePos = btree_write_node(tempNode);

	if (BTREE_IS_LEAF(rootNode))
	{
		rootNode->keyCount = div + 1;
		rootNode->children[(int)rootNode->keyCount] = *filePos;
	}
	else
	{
		rootNode->keyCount = div;

		//rootNode->keys[(int)rootNode->keyCount]     = 0 ;
		btree_keyset(btree_key(rootNode->keys, (int)rootNode->keyCount, tree->s_key), 0, tree->s_key) ;
	}

	BTB_SET_DIRTY(rootNode->block);
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
	uint64_t   temp1[tree->s_key], temp2[tree->s_key];
	offset_t   offset1[tree->s_value], offset2[tree->s_value];
	//char       tempSize1, tempSize2;
	int        i, j, div;

	assert(BTREE_IS_LEAF(rootNode)) ;
	// XXX: to be removed when have val sizes > 1 
	//assert(btree_leaf_order(rootNode) == tree->order) ;

	for (i = 0;
		i < (btree_leaf_order(rootNode) - 1) && 
		btree_keycmp(key, btree_key(rootNode->keys, i, tree->s_key), tree->s_key) > 0 ;
		i++)
		;

	if (i < (btree_leaf_order(rootNode) - 1) && 
		btree_keycmp(key, btree_key(rootNode->keys, i, tree->s_key), tree->s_key) == 0 )  //cmp
	{
		*split = 0;
		return 0;
	}

	*split = 1;
	
	if (i < (btree_leaf_order(rootNode) - 1))
	{
		//temp1                 = rootNode->keys[i];
		btree_keycpy(temp1, btree_key(rootNode->keys, i, tree->s_key), tree->s_key) ;
		//rootNode->keys[i]     = *key;
		btree_keycpy(btree_key(rootNode->keys, i, tree->s_key) , key, tree->s_key) ;
		j = i;

		for (i++; i < (btree_leaf_order(rootNode) - 1); i++)
		{
			//temp2     = rootNode->keys[i];
			btree_keycpy(temp2, btree_key(rootNode->keys, i , tree->s_key), tree->s_key) ;
			
			//rootNode->keys[i]     = temp1;
			btree_keycpy(btree_key(rootNode->keys, i, tree->s_key), temp1, tree->s_key) ;
			
			//temp1     = temp2;
			btree_keycpy(temp1, temp2, tree->s_key) ;
			//tempSize1 = tempSize2;
		}

		if (!BTREE_IS_LEAF(rootNode))
			j++;

		
		//offset1 = rootNode->children[j];
		btree_valcpy(offset1, 
					 btree_value(rootNode->children, j, tree->s_value), 
					 tree->s_value) ;

		//rootNode->children[j] = *filePos;
		btree_valcpy(btree_value(rootNode->children, j,tree->s_value), 
			 		 filePos, 
			 		 tree->s_value) ;
			
		
		
		for (j++; j <= (btree_leaf_order(rootNode) - 1); j++)
		{
			//offset2 = rootNode->children[j];
			btree_valcpy(offset2, 
					 btree_value(rootNode->children, j, tree->s_value), 
					 tree->s_value) ;
			//rootNode->children[j] = offset1;
			btree_valcpy(btree_value(rootNode->children, j, tree->s_value), 
					 offset1, 
					 tree->s_value) ;
			//offset1 = offset2;
			btree_valcpy(offset1, offset2, tree->s_value) ;
		}
	}
	else
	{
		//temp1     = *key;
		btree_keycpy(temp1, key, tree->s_key) ;
		//tempSize1 = sizeof(bt_key_t) ;

		if (BTREE_IS_LEAF(rootNode))
		{
			//offset1 = rootNode->children[btree_leaf_order(rootNode) - 1];
			btree_valcpy(offset1, 
						 btree_value(rootNode->children, 
						 			 btree_leaf_order(rootNode) - 1, 
						 			 tree->s_value), 
			 			 tree->s_value) ;
			//rootNode->children[btree_leaf_order(rootNode) - 1] = *filePos;
			btree_valcpy(btree_value(rootNode->children, 
						 			 btree_leaf_order(rootNode) - 1, 
						 			 tree->s_value), 
			 			 filePos,
			 			 tree->s_value) ;
			
		}
		else
			btree_valcpy(offset1, filePos, tree->s_value) ;
			//offset1 = *filePos;
	}

	if (BTREE_IS_LEAF(rootNode))
		div = (int)((btree_leaf_order(rootNode) + 1) / 2) - 1;
	else
		div = (int)(btree_leaf_order(rootNode) / 2);

	//*key = rootNode->keys[div] ;
	btree_keycpy(key, btree_key(rootNode->keys, div, tree->s_key), tree->s_key) ;
	
	tempNode           = btree_new_node(tree);
	tempNode->keyCount = btree_leaf_order(rootNode) - 1 - div;

	if (BTREE_IS_LEAF(rootNode))
		BTREE_SET_LEAF(tempNode);

	i = div + 1;

	for (j = 0; j < tempNode->keyCount - 1; j++, i++)
	{
		//tempNode->keys[j]     = rootNode->keys[i];
		btree_keycpy(btree_key(tempNode->keys, j, tree->s_key), 
					 btree_key(rootNode->keys, i, tree->s_key), 
					 tree->s_key) ;
			
		//tempNode->children[j] = rootNode->children[i];
		const offset_t *temp_child = btree_value(tempNode->children, j, tree->s_value) ;
		const offset_t *root_child = btree_value(rootNode->children, i, tree->s_value) ;
		btree_valcpy(temp_child, root_child, tree->s_value) ;

		//rootNode->keys[i]     = 0;
		btree_keyset(btree_key(rootNode->keys, i, tree->s_key), 0, tree->s_key) ;
		//rootNode->children[i] = 0;
		btree_valset(btree_value(rootNode->children, i, tree->s_value), 
					 0,
					 tree->s_value) ;
	}

	//tempNode->keys[j]         = temp1;
	btree_keycpy(btree_key(tempNode->keys, j, tree->s_key), temp1, tree->s_key) ;
	//tempNode->children[j]     = rootNode->children[i];
	const offset_t *temp_child = btree_value(tempNode->children, j, tree->s_value) ;
	const offset_t *root_child = btree_value(rootNode->children, i, tree->s_value) ;
	btree_valcpy(temp_child, root_child, tree->s_value) ;
	
	//rootNode->children[i]     = 0;
	btree_valset(btree_value(rootNode->children, i, tree->s_value), 0, tree->s_value) ;
	
	//tempNode->children[j + 1] = offset1;
	btree_valcpy(btree_value(tempNode->children, j + 1, tree->s_value), 
				 offset1,
				 tree->s_value) ;
	
	//*filePos = btree_write_node(tempNode);
	btree_valset(filePos, 0, tree->s_value) ;
	*filePos = btree_write_node(tempNode);

	if (BTREE_IS_LEAF(rootNode))
	{
		rootNode->keyCount = div + 1;
		//rootNode->children[(int)rootNode->keyCount] = *filePos;
		btree_valcpy(btree_value(rootNode->children, 
								(int)rootNode->keyCount, 
								tree->s_value),
					 filePos,
					 tree->s_value) ;
					 
	}
	else
	{
		rootNode->keyCount = div;

		//rootNode->keys[(int)rootNode->keyCount]     = 0 ;
		btree_keyset(btree_key(rootNode->keys, (int)rootNode->keyCount, tree->s_key), 0, tree->s_key) ;
	}

	BTB_SET_DIRTY(rootNode->block);
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
	uint64_t  temp1[tree->s_key], temp2[tree->s_key];
	offset_t  offset1, offset2;
	char      tempSize1, tempSize2;
	int       i, j;

	assert(!BTREE_IS_LEAF(rootNode)) ;

	*split = 0;

	for (i = 0;
		 i < rootNode->keyCount && 
		 btree_keycmp(key, btree_key(rootNode->keys, i, tree->s_key), tree->s_key) > 0 ;  // cmp
		 i++)
		;

	
	if (i < rootNode->keyCount && 
		btree_keycmp(key, btree_key(rootNode->keys, i, tree->s_key), tree->s_key) == 0 )  // cmp
	{
		return 0;
	}
	

	rootNode->keyCount++;

	if (i < rootNode->keyCount)
	{
		//temp1     = rootNode->keys[i];
		btree_keycpy(temp1, btree_key(rootNode->keys, i, tree->s_key), tree->s_key) ;

		//rootNode->keys[i]     = *key ;
		btree_keycpy(btree_key(rootNode->keys, i, tree->s_key), key, tree->s_key) ;
		
		j = i;
		
		for (i++; i < rootNode->keyCount; i++)
		{
			//temp2     = rootNode->keys[i];
			btree_keycpy(temp2, btree_key(rootNode->keys, i, tree->s_key), tree->s_key) ;

			//rootNode->keys[i]     = temp1;
			btree_keycpy(btree_key(rootNode->keys, i, tree->s_key), temp1, tree->s_key) ;

			//temp1     = temp2;
			btree_keycpy(temp1, temp2, tree->s_key) ;
			tempSize1 = tempSize2;
		}

		if (!BTREE_IS_LEAF(rootNode))
			j++;

		offset1 = rootNode->children[j];
		rootNode->children[j] = *filePos;
		
		for (j++; j <= rootNode->keyCount; j++)
		{
			offset2 = rootNode->children[j];
			rootNode->children[j] = offset1;
			offset1 = offset2;
		}
	}
	else
	{
		//rootNode->keys[i]     = *key ;
		btree_keycpy(btree_key(rootNode->keys, i, tree->s_key), key, tree->s_key) ;
	
		if (BTREE_IS_LEAF(rootNode))
		{
			rootNode->children[i + 1] = rootNode->children[i];
			rootNode->children[i]     = *filePos;
		}
		else
			rootNode->children[i + 1] = *filePos;
	}

	BTB_SET_DIRTY(rootNode->block);
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
	uint64_t  temp1[tree->s_key], temp2[tree->s_key];
	offset_t  offset1[tree->s_value], offset2[tree->s_value];
	char      tempSize1, tempSize2;
	int       i, j;

	assert(BTREE_IS_LEAF(rootNode)) ;

	*split = 0;

	for (i = 0;
		 i < rootNode->keyCount && 
		 btree_keycmp(key, btree_key(rootNode->keys, i, tree->s_key), tree->s_key) > 0 ;  // cmp
		 i++)
		;

	
	if (i < rootNode->keyCount && 
		btree_keycmp(key, btree_key(rootNode->keys, i, tree->s_key), tree->s_key) == 0 )  // cmp
	{
		return 0;
	}
	

	rootNode->keyCount++;

	if (i < rootNode->keyCount)
	{
		//temp1     = rootNode->keys[i];
		btree_keycpy(temp1, btree_key(rootNode->keys, i, tree->s_key), tree->s_key) ;

		//rootNode->keys[i]     = *key ;
		btree_keycpy(btree_key(rootNode->keys, i, tree->s_key), key, tree->s_key) ;
		
		j = i;
		
		for (i++; i < rootNode->keyCount; i++)
		{
			//temp2     = rootNode->keys[i];
			btree_keycpy(temp2, btree_key(rootNode->keys, i, tree->s_key), tree->s_key) ;

			//rootNode->keys[i]     = temp1;
			btree_keycpy(btree_key(rootNode->keys, i, tree->s_key), temp1, tree->s_key) ;

			//temp1     = temp2;
			btree_keycpy(temp1, temp2, tree->s_key) ;
			tempSize1 = tempSize2;
		}

		if (!BTREE_IS_LEAF(rootNode))
			j++;

		//offset1 = rootNode->children[j];
		btree_valcpy(offset1, 
					 btree_value(rootNode->children, j, tree->s_value), 
					 tree->s_value) ;
		
		//rootNode->children[j] = *filePos;
		btree_valcpy(btree_value(rootNode->children, j, tree->s_value),
					 filePos,
					 tree->s_value) ;
		
		for (j++; j <= rootNode->keyCount; j++)
		{
			//offset2 = rootNode->children[j];
			btree_valcpy(offset2, 
						 btree_value(rootNode->children, j, tree->s_value), 
						 tree->s_value) ;
			//rootNode->children[j] = offset1;
			btree_valcpy(btree_value(rootNode->children, j, tree->s_value), 
						 offset1,
						 tree->s_value) ;
			//offset1 = offset2;
			btree_valcpy(offset1, offset2, tree->s_value) ;
		}
	}
	else
	{
		//rootNode->keys[i]     = *key ;
		btree_keycpy(btree_key(rootNode->keys, i, tree->s_key), key, tree->s_key) ;
	
		if (BTREE_IS_LEAF(rootNode))
		{
			//rootNode->children[i + 1] = rootNode->children[i];
			const offset_t *temp1 = btree_value(rootNode->children, i + 1, tree->s_key) ;
			const offset_t *temp2 = btree_value(rootNode->children, i, tree->s_key) ;
			btree_valcpy(temp1, temp2, tree->s_value) ;
			//rootNode->children[i]     = *filePos;
			btree_valcpy(btree_value(rootNode->children, i, tree->s_value) ,
						 filePos,
						 tree->s_value) ;
		}
		else
			//rootNode->children[i + 1] = *filePos;
			btree_valcpy(btree_value(rootNode->children, i + 1, tree->s_value) ,
						 filePos,
						 tree->s_value) ;
	}

	BTB_SET_DIRTY(rootNode->block);
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

	rootNode = bt_read_node(tree, rootOffset);

	if (BTREE_IS_LEAF(rootNode))
	{
		//assert(tree->order == btree_leaf_order(rootNode)) ;
		
		if (rootNode->keyCount < (btree_leaf_order(rootNode) - 1))
			success = __addKeyToLeaf(tree, rootNode, key, filePos, split);
			//success = __addKey(tree, rootNode, key, filePos, split);
		else
			success = __splitLeaf(tree, rootNode, key, filePos, split);
			//success = __splitNode(tree, rootNode, key, filePos, split);

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
btree_insert(struct btree * tree, const uint64_t *key, offset_t *val)
{
	char  success, split;
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

	tree->_insFilePos = v ;
	
	/* Read in the tree data. */
	// XXX: remove?
	tree->root     = bt_root_node(tree);
	tree->left_leaf = bt_left_leaf(tree);
	tree->size     = bt_tree_size(tree);

	if (tree->root != 0)
	{
		success = __insertKey(tree, tree->root, k, tree->_insFilePos,
							  &split);

		if (success == 0)
		{
			btree_release_nodes(tree) ;
			// duplicate
			return -E_INVAL;;
		}
	}
	
	bt_tree_size_is(tree, tree->size + 1) ;

	if (tree->root == 0 || split == 1)
	{
		struct btree_node *node = btree_new_node(tree);

		//node->keys[0]     = key ;
		btree_keycpy(btree_key(node->keys, 0, tree->s_key), k, tree->s_key) ;
		node->keyCount    = 1;

		if (tree->root == 0)
		{
			BTREE_SET_LEAF(node);
			//node->children[0] = tree->_insFilePos;
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

		btree_root_node_is(tree, node->block.offset);
		btree_destroy_node(node);
	}

	btree_release_nodes(tree) ;
	return 0;	
}
