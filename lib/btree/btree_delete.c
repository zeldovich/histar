/**
 * @file btree_delete.c Deletion functions
 * 
 * $Id: btree_delete.c,v 1.5 2002/04/07 18:29:40 chipx86 Exp $
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
__removeKey(struct btree *tree, 
			struct btree_node *rootNode, 
			const uint64_t *key,
			offset_t *filePos)
{
	int i;
	
	assert(BTREE_IS_LEAF(rootNode)) ;
	
	for (i = 0;
		 i < rootNode->keyCount && 
		 btree_keycmp(btree_key(rootNode->keys, i, tree->s_key), key, tree->s_key) < 0;
		 i++)
		;

	if (i < rootNode->keyCount &&
		btree_keycmp(btree_key(rootNode->keys, i, tree->s_key), key, tree->s_key) == 0)
	{
		btree_valcpy(filePos, 
					 btree_value(rootNode->children, i, tree->s_key), 
					 tree->s_key) ;

		// Move keys i+1 through keyCount-1 into i through keyCount-2
		btree_keymove(btree_key(rootNode->keys, i,   tree->s_key),
			      btree_key(rootNode->keys, i+1, tree->s_key),
			      tree->s_key * (rootNode->keyCount - i - 1));

		// Move values i+1 through keyCount-1 into i through keyCount-2
		btree_valmove(btree_value(rootNode->children, i,   tree->s_value),
			      btree_value(rootNode->children, i+1, tree->s_value),
			      tree->s_value * (rootNode->keyCount - i - 1));

		i = rootNode->keyCount - 1;
		btree_keyset(btree_key(rootNode->keys, i, tree->s_key), 0, tree->s_key) ;

		const offset_t *temp1 = btree_value(rootNode->children, i, tree->s_value) ;
		const offset_t *temp2 = btree_value(rootNode->children, i + 1, tree->s_value) ;
		btree_valcpy(temp1, temp2, tree->s_value) ;

		btree_valset(btree_value(rootNode->children, i + 1, tree->s_value),
					 0,
					 tree->s_value) ;
		
		rootNode->keyCount--;

		BTB_SET_DIRTY(rootNode->block);

		btree_write_node(rootNode);

		return 1;
	}

	return 0;
}

static void
__removeKey2(struct btree *tree, 
			 struct btree_node *rootNode, 
			 int index)
{
	int i;

	assert(!BTREE_IS_LEAF(rootNode)) ;
    
    // Move keys index+1 through keyCount-1 to index through keyCount-2 
    btree_keymove(btree_key(rootNode->keys, index, tree->s_key), 
                 btree_key(rootNode->keys, index + 1, tree->s_key), 
                 tree->s_key * (rootNode->keyCount - 1 - index)) ;

    // Move ptrs index+1 through keyCount to index through keyCount-1
    for (i = index; i < rootNode->keyCount ; i++)
        rootNode->children[i] = rootNode->children[i + 1];
    
    rootNode->children[rootNode->keyCount] = 0;

	btree_keyset(btree_key(rootNode->keys, rootNode->keyCount - 1, tree->s_key), 
                 0, 
                 tree->s_key) ;

	rootNode->keyCount--;

	BTB_SET_DIRTY(rootNode->block);

	btree_write_node(rootNode);
}

static void
__removeKeyLeaf2(struct btree *tree, 
			 	  struct btree_node *rootNode, 
			 	  int index)
{
	assert(BTREE_IS_LEAF(rootNode)) ;

	// Move keys index+1 through keyCount-1 to index through keyCount-2 
    btree_keymove(btree_key(rootNode->keys, index, tree->s_key), 
                 btree_key(rootNode->keys, index + 1, tree->s_key), 
                 tree->s_key * (rootNode->keyCount - 1 - index)) ;

    // Move values index+1 through keyCount to index through keyCount-1
    btree_valmove(btree_value(rootNode->children, index, tree->s_value),
                  btree_value(rootNode->children, index + 1, tree->s_value),
                  tree->s_value * (rootNode->keyCount - index)) ;
    
	btree_keyset(btree_key(rootNode->keys, rootNode->keyCount - 1, tree->s_key), 
                 0, 
                 tree->s_key) ;
	
	btree_valset(btree_value(rootNode->children, rootNode->keyCount, tree->s_value),
				 0,
				 tree->s_value) ;

	rootNode->keyCount--;

	BTB_SET_DIRTY(rootNode->block);

	btree_write_node(rootNode);
}

static char
__borrowRight(struct btree *tree, 
			  struct btree_node *rootNode, 
			  struct btree_node *prevNode, 
			  int div)
{
	struct btree_node *node;

	assert(!BTREE_IS_LEAF(rootNode)) ;

	if (div >= prevNode->keyCount)
		return 0;

	node = btree_read_node(tree, prevNode->children[div + 1]);

	if (node->keyCount > tree->min_intrn)
	{
		btree_keycpy(btree_key(rootNode->keys, (int)rootNode->keyCount, tree->s_key),  
					 btree_key(prevNode->keys, div, tree->s_key),  
					 tree->s_key) ;

		btree_keycpy(btree_key(prevNode->keys, div, tree->s_key), 
					 btree_key(node->keys, 0, tree->s_key), 
					 tree->s_key) ;

		rootNode->children[rootNode->keyCount + 1] = node->children[0];
	}
	else
	{
		btree_destroy_node(node);

		return 0;
	}

	BTB_SET_DIRTY(rootNode->block);
	BTB_SET_DIRTY(prevNode->block);

	rootNode->keyCount++;

	__removeKey2(tree, node, 0);

	btree_destroy_node(node);
	
	return 1;
}

static char
__borrowRightLeaf(struct btree *tree, 
			  	  struct btree_node *rootNode, 
			  	  struct btree_node *prevNode, 
			  	  int div)
{
	struct btree_node *node;

	assert(BTREE_IS_LEAF(rootNode)) ;

	if (div >= prevNode->keyCount)
		return 0;

	// ok, accessing twig children
	node = btree_read_node(tree, prevNode->children[div + 1]);

	if (BTREE_IS_LEAF(node) && node->keyCount > tree->min_leaf)
	{
		const offset_t *temp1 = btree_value(rootNode->children, 
										   rootNode->keyCount + 1, 
										   tree->s_value) ;
		const offset_t *temp2 = btree_value(rootNode->children,
										   rootNode->keyCount,
										   tree->s_value) ;
		btree_valcpy(temp1, temp2, tree->s_value) ;
		
		btree_keycpy(btree_key(rootNode->keys, (int)rootNode->keyCount, tree->s_key), 
					 btree_key(node->keys, 0, tree->s_key), 
					 tree->s_key) ;
		temp1 = btree_value(rootNode->children, 
							rootNode->keyCount, 
							tree->s_value) ;
		temp2 = btree_value(node->children,
							0,
							tree->s_value) ;
		btree_valcpy(temp1, temp2, tree->s_value) ;

		btree_keycpy(btree_key(prevNode->keys, div, tree->s_key),  
					 btree_key(rootNode->keys, (int)rootNode->keyCount, tree->s_key),
					 tree->s_key) ;
	}
	else
	{
		btree_destroy_node(node);

		return 0;
	}

	BTB_SET_DIRTY(rootNode->block);
	BTB_SET_DIRTY(prevNode->block);

	rootNode->keyCount++;

	__removeKeyLeaf2(tree, node, 0);

	btree_destroy_node(node);
	
	return 1;
}

static char
__borrowLeft(struct btree *tree, 
			 struct btree_node *rootNode, 
			 struct btree_node *prevNode, 
			 int div)
{
	int i;
	struct btree_node *node;

	assert(!BTREE_IS_LEAF(rootNode)) ;

	if (div == 0)
		return 0;

	node = btree_read_node(tree, prevNode->children[div - 1]);

	if (node->keyCount > tree->min_intrn)
	{
        // Move 0 through keyCount - 1 to 1 through keyCount
        btree_keymove(btree_key(rootNode->keys, 1, tree->s_key), 
                      btree_key(rootNode->keys, 0, tree->s_key),
                      tree->s_key * rootNode->keyCount) ;
            
		for (i = rootNode->keyCount ; i > 0; i--)
			rootNode->children[i + 1] = rootNode->children[i];

		rootNode->children[1] = rootNode->children[0];
		btree_keycpy(btree_key(rootNode->keys, 0, tree->s_key),  
					 btree_key(prevNode->keys, div - 1, tree->s_key),  
					 tree->s_key) ;
		rootNode->children[0] = node->children[(int)node->keyCount];

		rootNode->keyCount++;

		btree_keycpy(btree_key(prevNode->keys, div - 1, tree->s_key), 
					 btree_key(node->keys, node->keyCount - 1, tree->s_key),  
					 tree->s_key) ;
		
		node->children[(int)node->keyCount] = 0;

		btree_keyset(btree_key(node->keys, node->keyCount - 1, tree->s_key), 0, tree->s_key) ;
	}
	else
	{
		btree_destroy_node(node);
		
		return 0;
	}

	node->keyCount--;

	BTB_SET_DIRTY(rootNode->block);
	BTB_SET_DIRTY(prevNode->block);
	BTB_SET_DIRTY(node->block);

	btree_write_node(node);
	btree_destroy_node(node);

	return 1;
}

static char
__borrowLeftLeaf(struct btree *tree, 
			 struct btree_node *rootNode, 
			 struct btree_node *prevNode, 
			 int div)
{
	struct btree_node *node;

	assert(BTREE_IS_LEAF(rootNode)) ;

	if (div == 0)
		return 0;

	// ok, reading twig children
	node = btree_read_node(tree, prevNode->children[div - 1]);

	if (node->keyCount > tree->min_leaf)
	{
		
        // Move 0 through keyCount - 1 to 1 through keyCount
        btree_keymove(btree_key(rootNode->keys, 1, tree->s_key), 
                      btree_key(rootNode->keys, 0, tree->s_key),
                      tree->s_key * rootNode->keyCount) ;
        
        // Move 0 through keyCount to 1 through keycount + 1
        btree_valmove(btree_value(rootNode->children, 
                                  1, 
                                  tree->s_value),
                      btree_value(rootNode->children,
                                  0,
                                  tree->s_value),
                      tree->s_value * (rootNode->keyCount + 1)) ;
		
		btree_keycpy(btree_key(rootNode->keys, 0, tree->s_key), 
					 btree_key(node->keys, node->keyCount - 1, tree->s_key),
					 tree->s_key) ;

		const offset_t *temp1 = btree_value(rootNode->children, 0, tree->s_value) ;
		const offset_t *temp2 = btree_value(node->children, node->keyCount - 1, tree->s_value) ;
		btree_valcpy(temp1, temp2, tree->s_value) ;

		rootNode->keyCount++;

		btree_keycpy(btree_key(prevNode->keys, div - 1, tree->s_key),  
					 btree_key(node->keys, node->keyCount - 2, tree->s_key),  
					 tree->s_key) ;


		temp1 = temp2 ;
		temp2 = btree_value(node->children, node->keyCount, tree->s_value) ;
		btree_valcpy(temp1, temp2, tree->s_value) ;
		btree_valset(temp2, 0, tree->s_value) ;

		btree_keyset(btree_key(node->keys, node->keyCount - 1, tree->s_key), 
					 0, 
					 tree->s_key) ;
	}
	else
	{
		btree_destroy_node(node);
		
		return 0;
	}

	node->keyCount--;

	BTB_SET_DIRTY(rootNode->block);
	BTB_SET_DIRTY(prevNode->block);
	BTB_SET_DIRTY(node->block);

	btree_write_node(node);
	btree_destroy_node(node);

	return 1;
}

static char
__mergeNode(struct btree *tree, 
			struct btree_node *rootNode, 
			struct btree_node *prevNode, 
			int div)
{
	int i, j;
	struct btree_node *node;

	assert(!BTREE_IS_LEAF(rootNode)) ;

	/* Try to merge the node with its left sibling. */
	if (div > 0)
	{
		node = btree_read_node(tree, prevNode->children[div - 1]);
		i    = node->keyCount;

		btree_keycpy(btree_key(node->keys, i, tree->s_key), 
					 btree_key(prevNode->keys, div - 1, tree->s_key), 
					 tree->s_key) ;
		node->keyCount++;
		
		i++;

        
        btree_keycpy(btree_key(node->keys, i, tree->s_key), 
                     btree_key(rootNode->keys, 0, tree->s_key), 
                     tree->s_key * rootNode->keyCount) ;
        

		for (j = 0; j < rootNode->keyCount; j++, i++)
		{
			node->children[i] = rootNode->children[j];
			node->keyCount++;
		}
    

		node->children[i] = rootNode->children[j];

		BTB_SET_DIRTY(node->block);

		btree_write_node(node);
		
		prevNode->children[div] = node->block.offset;

		BTB_SET_DIRTY(prevNode->block);

		btree_erase_node(rootNode);
		__removeKey2(tree, prevNode, div - 1);
		
		btree_write_node(node);
		btree_write_node(prevNode);

		btree_destroy_node(node);
		
		
	}
	else
	{
		/* Must merge the node with its right sibling. */
		node = btree_read_node(tree, prevNode->children[div + 1]);
		i    = rootNode->keyCount;

		btree_keycpy(btree_key(rootNode->keys, i, tree->s_key), 
					 btree_key(prevNode->keys, div, tree->s_key),  
					 tree->s_key) ;
		rootNode->keyCount++;
		
		i++;

		
        btree_keycpy(btree_key(rootNode->keys, i, tree->s_key), 
                     btree_key(node->keys, 0, tree->s_key),
                     tree->s_key * node->keyCount) ;
        
        for (j = 0; j < node->keyCount; j++, i++)
		{
			rootNode->children[i] = node->children[j];
			rootNode->keyCount++;
		}
        
		rootNode->children[i]       = node->children[j];
		prevNode->children[div + 1] = rootNode->block.offset;

		BTB_SET_DIRTY(rootNode->block);
		BTB_SET_DIRTY(prevNode->block);

		btree_erase_node(node);

		__removeKey2(tree, prevNode, div);
		
		btree_write_node(prevNode);
		btree_write_node(rootNode);
	}

	return 1;
}

static char __attribute__((noinline))
__mergeLeaf(struct btree *tree, 
			struct btree_node *rootNode, 
			struct btree_node *prevNode, 
			int div)
{
	int i ;
	struct btree_node *node;

	assert(BTREE_IS_LEAF(rootNode)) ;

	/* Try to merge the node with its left sibling. */
	if (div > 0)
	{
		node = btree_read_node(tree, prevNode->children[div - 1]);
		i    = node->keyCount;

        btree_keycpy(btree_key(node->keys, i, tree->s_key), 
                     btree_key(rootNode->keys, 0, tree->s_key), 
                     tree->s_key * rootNode->keyCount) ;
        
        btree_valcpy(btree_value(node->children, i, tree->s_value),
                     btree_value(rootNode->children, 0, tree->s_value),
                     tree->s_value * (rootNode->keyCount + 1)) ;

        node->keyCount += rootNode->keyCount ;
        
		BTB_SET_DIRTY(node->block);

		btree_write_node(node);
		
		// ok, accessing twig node children
		prevNode->children[div] = node->block.offset;

		BTB_SET_DIRTY(prevNode->block);

		btree_erase_node(rootNode);
		__removeKey2(tree, prevNode, div - 1);
		
		btree_write_node(node);
		btree_write_node(prevNode);

		btree_destroy_node(node);
	}
	else
	{
		/* Must merge the node with its right sibling. */
		node = btree_read_node(tree, prevNode->children[div + 1]);
		i    = rootNode->keyCount;
        
        btree_keycpy(btree_key(rootNode->keys, i, tree->s_key), 
                     btree_key(node->keys, 0, tree->s_key),
                     tree->s_key * node->keyCount) ;
        
        btree_valcpy(btree_value(rootNode->children, i, tree->s_value),
                     btree_value(node->children, 0, tree->s_value),
                     tree->s_value * (node->keyCount + 1)) ;
        rootNode->keyCount += node->keyCount ;

		// ok, accessing twig children
		prevNode->children[div + 1] = rootNode->block.offset;

		BTB_SET_DIRTY(rootNode->block);
		BTB_SET_DIRTY(prevNode->block);

		btree_erase_node(node);

		__removeKey2(tree, prevNode, div);
		
		btree_write_node(prevNode);
		btree_write_node(rootNode);
	}

	return 1;
}

static char
__delete(struct btree *tree, 
		 offset_t rootOffset, 
		 struct btree_node *prevNode,
		 const uint64_t *key, 
		 int index, 
		 offset_t *filePos, 
		 char *merged)
{
	char success = 0;
	struct btree_node *rootNode;

	rootNode = btree_read_node(tree, rootOffset);

	if (BTREE_IS_LEAF(rootNode))
	{
		success = __removeKey(tree, rootNode, key, filePos);
	}
	else
	{
		int i;
		
		for (i = 0;
			 i < rootNode->keyCount && 
			 btree_keycmp(btree_key(rootNode->keys, i, tree->s_key), key, tree->s_key) < 0;
			 i++)
			;

		success = __delete(tree, rootNode->children[i], rootNode, key, i,
						   filePos, merged);
	}

	if (success == 0)
	{
		btree_destroy_node(rootNode);
		
		return 0;
	}
	else if ((rootNode->block.offset == tree->root) ||
			 (BTREE_IS_LEAF(rootNode)  && rootNode->keyCount >= tree->min_leaf) ||
			 (!BTREE_IS_LEAF(rootNode) && rootNode->keyCount >= tree->min_intrn))
	{
		btree_destroy_node(rootNode);
		
		return 1;
	}
	else
	{
		if (BTREE_IS_LEAF(rootNode)) {
			if (__borrowRightLeaf(tree, rootNode, prevNode, index) ||
				__borrowLeftLeaf(tree, rootNode, prevNode, index))
			{
				*merged = 0;
				btree_write_node(rootNode);
				btree_write_node(prevNode);
			}
			else
			{
				*merged = 1;
				__mergeLeaf(tree, rootNode, prevNode, index);
			}
		}
		else {
			if (__borrowRight(tree, rootNode, prevNode, index) ||
				__borrowLeft(tree, rootNode, prevNode, index))
			{
				*merged = 0;
				btree_write_node(rootNode);
				btree_write_node(prevNode);
			}
			else
			{
				*merged = 1;
				__mergeNode(tree, rootNode, prevNode, index);
			}
		}
	}

	btree_destroy_node(rootNode);
	
	return 1;
}

char 
btree_delete(void *t, const uint64_t *key)
{
	struct btree *tree = (struct btree *)t ;
	assert(tree->magic == BTREE_MAGIC) ;
	
	int i;
	offset_t filePos[tree->s_value] ;
	char merged, success;
	struct btree_node *rootNode;

	if (tree == NULL || key == 0 || tree->root == 0)
		return -E_INVAL ;

	btree_lock(tree) ;

	btree_valset(filePos, 0, tree->s_value) ;
	merged  = 0;
	success = 0;

	
	/* Read in the root node. */
	rootNode = btree_read_node(tree, tree->root);
	
	for (i = 0;
		 i < rootNode->keyCount && 
		 btree_keycmp(btree_key(rootNode->keys, i, tree->s_key), key, tree->s_key) < 0;
		 i++)
		;

	success = __delete(tree, tree->root, NULL, key, i, filePos, &merged);

	if (success == 0)
	{
		btree_destroy_node(rootNode);
		btree_unlock(tree) ;
		return -E_INVAL;
	}
	
	bt_tree_size_is(tree, tree->size - 1);


	if (BTREE_IS_LEAF(rootNode) && rootNode->keyCount == 0)
	{
		btree_root_node_is(tree, 0);
                btree_height_is(tree, 0) ;
		btree_erase_node(rootNode);
		btree_unlock(tree) ;
		return 0 ;
	}
	else if (merged == 1 && rootNode->keyCount == 0)
	{
		struct btree_node *tempNode;
		
		btree_root_node_is(tree, rootNode->children[0]);

		tempNode = btree_read_node(tree, tree->root);

		BTB_SET_DIRTY(tempNode->block);

		btree_write_node(tempNode);
		btree_destroy_node(tempNode);

		btree_erase_node(rootNode);
                
        btree_height_is(tree, tree->height - 1) ;
                
		btree_unlock(tree) ;
		return 0 ;
	}
	btree_destroy_node(rootNode);
	btree_unlock(tree) ;
	return 0 ;
}



