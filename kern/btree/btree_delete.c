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

#include <btree/btree_impl.h>
#include <btree/btree_manager.h>
#include <btree/btree_utils.h>
#include <btree/btree_node.h>
#include <kern/lib.h>
#include <inc/error.h>

static char
__removeKey(struct btree *tree, struct btree_node *rootNode,
	    const uint64_t * key, offset_t * filePos)
{
    int i;

    assert(BTREE_IS_LEAF(rootNode));

    for (i = 0;
	 i < rootNode->keyCount
	 && btree_keycmp(btree_key(rootNode, i), key, tree->s_key) < 0; i++) ;

    if (i < rootNode->keyCount
	&& btree_keycmp(btree_key(rootNode, i), key, tree->s_key) == 0) {
	// *filePos = rootNode->children[i];
	btree_valcpy(filePos, btree_value(rootNode, i), tree->s_value);

	// Move keys i+1 through keyCount-1 into i through keyCount-2
	btree_keymove(btree_key(rootNode, i), btree_key(rootNode, i + 1),
		      tree->s_key * (rootNode->keyCount - i - 1));

	// Move values i+1 through keyCount-1 into i through keyCount-2
	btree_valmove(btree_value(rootNode, i), btree_value(rootNode, i + 1),
		      tree->s_value * (rootNode->keyCount - i - 1));

	i = rootNode->keyCount - 1;
	//rootNode->keys[i]         = 0;
	btree_keyset(btree_key(rootNode, i), 0, tree->s_key);

	//rootNode->children[i]     = rootNode->children[i + 1];
	bt_valcpy(rootNode, i, rootNode, i + 1);

	//rootNode->children[i + 1] = 0;
	btree_valset(btree_value(rootNode, i + 1), 0, tree->s_value);

	rootNode->keyCount--;

	btree_write_node(rootNode);

	return 1;
    }

    return 0;
}

static void
__removeKey2(struct btree *tree, struct btree_node *rootNode, int index)
{
    int i;

    assert(!BTREE_IS_LEAF(rootNode));

    for (i = index; i < rootNode->keyCount - 1; i++) {
	//rootNode->keys[i]     = rootNode->keys[i + 1];
	bt_keycpy(rootNode, i, rootNode, i + 1);
	rootNode->children[i] = rootNode->children[i + 1];
    }

    //rootNode->keys[i]         = 0;
    btree_keyset(btree_key(rootNode, i), 0, tree->s_key);
    rootNode->children[i] = rootNode->children[i + 1];
    rootNode->children[i + 1] = 0;

    rootNode->keyCount--;

    btree_write_node(rootNode);
}

static void
__removeKeyLeaf2(struct btree *tree, struct btree_node *rootNode, int index)
{
    int i;

    assert(BTREE_IS_LEAF(rootNode));

    for (i = index; i < rootNode->keyCount - 1; i++) {
	//rootNode->keys[i]     = rootNode->keys[i + 1];
	bt_keycpy(rootNode, i, rootNode, i + 1);

	//rootNode->children[i] = rootNode->children[i + 1];
	bt_valcpy(rootNode, i, rootNode, i + 1);
    }

    //rootNode->keys[i]         = 0;
    btree_keyset(btree_key(rootNode, i), 0, tree->s_key);

    //rootNode->children[i]     = rootNode->children[i + 1];
    bt_valcpy(rootNode, i, rootNode, i + 1);

    //rootNode->children[i + 1] = 0;
    btree_valset(btree_value(rootNode, i + 1), 0, tree->s_value);

    rootNode->keyCount--;

    btree_write_node(rootNode);
}

static char
__borrowRight(struct btree *tree, struct btree_node *rootNode,
	      struct btree_node *prevNode, int div)
{
    struct btree_node *node;

    assert(!BTREE_IS_LEAF(rootNode));

    if (div >= prevNode->keyCount)
	return 0;

    node = btree_read_node(tree, prevNode->children[div + 1]);

    if (node->keyCount > tree->min_intrn) {
	//rootNode->keys[rootNode->keyCount] = prevNode->keys[div];
	bt_keycpy(rootNode, rootNode->keyCount, prevNode, div);

	//prevNode->keys[div]                         = node->keys[0] ;
	bt_keycpy(prevNode, div, node, 0);

	rootNode->children[rootNode->keyCount + 1] = node->children[0];
    } else {
	btree_destroy_node(node);

	return 0;
    }

    rootNode->keyCount++;

    __removeKey2(tree, node, 0);

    btree_destroy_node(node);

    return 1;
}

static char
__borrowRightLeaf(struct btree *tree, struct btree_node *rootNode,
		  struct btree_node *prevNode, int div)
{
    struct btree_node *node;

    assert(BTREE_IS_LEAF(rootNode));

    if (div >= prevNode->keyCount)
	return 0;

    // ok, accessing twig children
    node = btree_read_node(tree, prevNode->children[div + 1]);

    if (node->keyCount > tree->min_leaf) {
	//rootNode->children[rootNode->keyCount + 1] =
	//      rootNode->children[rootNode->keyCount];
	bt_valcpy(rootNode, rootNode->keyCount + 1, rootNode,
		  rootNode->keyCount);

	//rootNode->keys[rootNode->keyCount]     = node->keys[0] ;
	bt_keycpy(rootNode, rootNode->keyCount, node, 0);

	//rootNode->children[rootNode->keyCount] = node->children[0];
	bt_valcpy(rootNode, rootNode->keyCount, node, 0);

	//prevNode->keys[div] = rootNode->keys[rootNode->keyCount];
	bt_keycpy(prevNode, div, rootNode, rootNode->keyCount);
    } else {
	btree_destroy_node(node);
	return 0;
    }

    rootNode->keyCount++;

    __removeKeyLeaf2(tree, node, 0);

    btree_destroy_node(node);

    return 1;
}

static char
__borrowLeft(struct btree *tree, struct btree_node *rootNode,
	     struct btree_node *prevNode, int div)
{
    int i;
    struct btree_node *node;

    assert(!BTREE_IS_LEAF(rootNode));

    if (div == 0)
	return 0;

    node = btree_read_node(tree, prevNode->children[div - 1]);

    if (node->keyCount > tree->min_intrn) {
        for (i = rootNode->keyCount; i > 0; i--) {
            //rootNode->keys[i]         = rootNode->keys[i - 1];
            bt_keycpy(rootNode, i, rootNode, i - 1);
            rootNode->children[i + 1] = rootNode->children[i];
        }
        
        rootNode->children[1] = rootNode->children[0];
        //rootNode->keys[0]     = prevNode->keys[div - 1];
        bt_keycpy(rootNode, 0, prevNode, div - 1);
        rootNode->children[0] = node->children[node->keyCount];
        
        rootNode->keyCount++;
        
        //prevNode->keys[div - 1]     = node->keys[node->keyCount - 1];
        bt_keycpy(prevNode, div - 1, node, node->keyCount - 1);
        
        node->children[node->keyCount] = 0;
        
        //node->keys[node->keyCount - 1]     = 0;
        btree_keyset(btree_key(node, node->keyCount - 1), 0, tree->s_key);
    } else {
        btree_destroy_node(node);
        return 0;
    }

    node->keyCount--;

    btree_write_node(node);
    btree_destroy_node(node);

    return 1;
}

static char
__borrowLeftLeaf(struct btree *tree, struct btree_node *rootNode,
		 struct btree_node *prevNode, int div)
{
    int i;
    struct btree_node *node;

    assert(BTREE_IS_LEAF(rootNode));

    if (div == 0)
	return 0;

    // ok, reading twig children
    node = btree_read_node(tree, prevNode->children[div - 1]);

    if (node->keyCount > tree->min_leaf) {
	for (i = rootNode->keyCount; i > 0; i--) {
	    //rootNode->keys[i]         = rootNode->keys[i - 1];
	    bt_keycpy(rootNode, i, rootNode, i - 1);

	    //rootNode->children[i + 1] = rootNode->children[i];
	    bt_valcpy(rootNode, i + 1, rootNode, i);
	}

	//rootNode->children[1] = rootNode->children[0];
	bt_valcpy(rootNode, 1, rootNode, 0);

	//rootNode->keys[0]     = node->keys[node->keyCount - 1];
	bt_keycpy(rootNode, 0, node, node->keyCount - 1);

	//rootNode->children[0] = node->children[node->keyCount - 1];
	bt_valcpy(rootNode, 0, node, node->keyCount - 1);

	rootNode->keyCount++;

	//prevNode->keys[div - 1]     = node->keys[node->keyCount - 2];
	bt_keycpy(prevNode, div - 1, node, node->keyCount - 2);

	//node->children[node->keyCount - 1] =
	//      node->children[node->keyCount];
	//node->children[node->keyCount] = 0;
	bt_valcpy(node, node->keyCount - 1, node, node->keyCount);
	btree_valset(btree_value(node, node->keyCount), 0, tree->s_value);

	//node->keys[node->keyCount - 1]     = 0;
	btree_keyset(btree_key(node, node->keyCount - 1), 0, tree->s_key);
    } else {
	btree_destroy_node(node);
	return 0;
    }

    node->keyCount--;

    btree_write_node(node);
    btree_destroy_node(node);

    return 1;
}

static char
__mergeNode(struct btree *tree, struct btree_node *rootNode,
	    struct btree_node *prevNode, int div)
{
    int i, j;
    struct btree_node *node;

    assert(!BTREE_IS_LEAF(rootNode));

    /* Try to merge the node with its left sibling. */
    if (div > 0) {
        node = btree_read_node(tree, prevNode->children[div - 1]);
        i = node->keyCount;

        //node->keys[i]     = prevNode->keys[div - 1];
        bt_keycpy(node, i, prevNode, div - 1);
        node->keyCount++;
        
        i++;

	for (j = 0; j < rootNode->keyCount; j++, i++) {
	    //node->keys[i]     = rootNode->keys[j];
	    bt_keycpy(node, i, rootNode, j);
	    node->children[i] = rootNode->children[j];
	    node->keyCount++;
	}

	node->children[i] = rootNode->children[j];

	btree_write_node(node);

	prevNode->children[div] = node->block.offset;

	btree_erase_node(rootNode);
	__removeKey2(tree, prevNode, div - 1);

	btree_write_node(node);
	btree_write_node(prevNode);

	btree_destroy_node(node);
    } else {
	/* Must merge the node with its right sibling. */
	node = btree_read_node(tree, prevNode->children[div + 1]);
	i = rootNode->keyCount;

        //rootNode->keys[i]     = prevNode->keys[div];
        bt_keycpy(rootNode, i, prevNode, div);
        rootNode->keyCount++;
        
        i++;

	for (j = 0; j < node->keyCount; j++, i++) {
	    //rootNode->keys[i]     = node->keys[j];
	    bt_keycpy(rootNode, i, node, j);
	    rootNode->children[i] = node->children[j];
	    rootNode->keyCount++;
	}

	rootNode->children[i] = node->children[j];
	prevNode->children[div + 1] = rootNode->block.offset;

	btree_erase_node(node);

	__removeKey2(tree, prevNode, div);

	btree_write_node(prevNode);
	btree_write_node(rootNode);
    }

    //btree_write_node(node);
    //btree_write_node(prevNode);
    //btree_write_node(rootNode);

    //btree_destroy_node(node);

    return 1;
}

static char
__mergeLeaf(struct btree *tree, struct btree_node *rootNode,
	    struct btree_node *prevNode, int div)
{
    int i, j;
    struct btree_node *node;

    assert(BTREE_IS_LEAF(rootNode));

    /* Try to merge the node with its left sibling. */
    if (div > 0) {
	node = btree_read_node(tree, prevNode->children[div - 1]);
	i = node->keyCount;

	for (j = 0; j < rootNode->keyCount; j++, i++) {
	    //node->keys[i]     = rootNode->keys[j];
	    bt_keycpy(node, i, rootNode, j);

	    //node->children[i] = rootNode->children[j];
	    bt_valcpy(node, i, rootNode, j);

	    node->keyCount++;
	}

	//node->children[i] = rootNode->children[j];
	bt_valcpy(node, i, rootNode, j);

	btree_write_node(node);

	// ok, accessing twig node children
	prevNode->children[div] = node->block.offset;

	btree_erase_node(rootNode);
	__removeKey2(tree, prevNode, div - 1);

	btree_write_node(node);
	btree_write_node(prevNode);

	btree_destroy_node(node);


    } else {
	/* Must merge the node with its right sibling. */
	node = btree_read_node(tree, prevNode->children[div + 1]);
	i = rootNode->keyCount;

	for (j = 0; j < node->keyCount; j++, i++) {
	    //rootNode->keys[i]     = node->keys[j];
	    bt_keycpy(rootNode, i, node, j);
	    //rootNode->children[i] = node->children[j];
	    bt_valcpy(rootNode, i, node, j);

	    rootNode->keyCount++;
	}

	//rootNode->children[i]       = node->children[j];
	bt_valcpy(rootNode, i, node, j);

	// ok, accessing twig children
	prevNode->children[div + 1] = rootNode->block.offset;

	btree_erase_node(node);

	__removeKey2(tree, prevNode, div);

	btree_write_node(prevNode);
	btree_write_node(rootNode);
    }

    return 1;
}

static char
__delete(struct btree *tree, offset_t rootOffset, struct btree_node *prevNode,
	 const uint64_t * key, int index, offset_t * filePos, char *merged)
{
    char success = 0;
    struct btree_node *rootNode;

    rootNode = btree_read_node(tree, rootOffset);

    if (BTREE_IS_LEAF(rootNode)) {
	success = __removeKey(tree, rootNode, key, filePos);
    } else {
	int i;

	for (i = 0;
	     i < rootNode->keyCount
	     && btree_keycmp(btree_key(rootNode, i), key, tree->s_key) < 0;
	     i++) ;

	success =
	    __delete(tree, rootNode->children[i], rootNode, key, i, filePos,
		     merged);
    }

    if (success == 0) {
	btree_destroy_node(rootNode);

	return 0;
    } else if ((rootNode->block.offset == tree->root)
	       || (BTREE_IS_LEAF(rootNode)
		   && rootNode->keyCount >= tree->min_leaf)
	       || (!BTREE_IS_LEAF(rootNode)
		   && rootNode->keyCount >= tree->min_intrn)) {
	btree_destroy_node(rootNode);

	return 1;
    } else {
	if (BTREE_IS_LEAF(rootNode)) {
	    if (__borrowRightLeaf(tree, rootNode, prevNode, index)
		|| __borrowLeftLeaf(tree, rootNode, prevNode, index)) {
		*merged = 0;
		btree_write_node(rootNode);
		btree_write_node(prevNode);
	    } else {
		*merged = 1;
		__mergeLeaf(tree, rootNode, prevNode, index);
	    }
	} else {
	    if (__borrowRight(tree, rootNode, prevNode, index)
		|| __borrowLeft(tree, rootNode, prevNode, index)) {
		*merged = 0;
		btree_write_node(rootNode);
		btree_write_node(prevNode);
	    } else {
		*merged = 1;
		__mergeNode(tree, rootNode, prevNode, index);
	    }
	}

	//btree_write_node(rootNode);
	//btree_write_node(prevNode);

    }

    btree_destroy_node(rootNode);

    return 1;
}

char
btree_delete_impl(struct btree *tree, const uint64_t * key)
{
    assert(tree->magic == BTREE_MAGIC);

    int i;
    offset_t filePos[tree->s_value];
    char merged, success;
    struct btree_node *rootNode;

    if (tree == 0 || key == 0) {
	cprintf("btree_delete_impl: null tree (%p) or key (%p)\n", tree, key);
	return -E_INVAL;
    }

    if (tree->root == 0)
	return -E_NOT_FOUND;

    btree_lock(tree->id);

    //filePos = 0;
    btree_valset(filePos, 0, tree->s_value);
    merged = 0;
    success = 0;

    cprintf("root refs %d\n", btree_refs_node(tree->id, tree->root));
    
    /* Read in the root node. */
    rootNode = btree_read_node(tree, tree->root);
    
    for (i = 0;
	 i < rootNode->keyCount
	 && btree_keycmp(btree_key(rootNode, i), key, tree->s_key) < 0; i++) ;
    
    cprintf("calling __delete...\n");
    success = __delete(tree, tree->root, NULL, key, i, filePos, &merged);
    cprintf("__delete done!\n");
    
    if (success == 0) {
	btree_destroy_node(rootNode);
	btree_unlock(tree->id);
	return -E_NOT_FOUND;
    }

    tree->size--;

    if (BTREE_IS_LEAF(rootNode) && rootNode->keyCount == 0) {
	cprintf("here\n");
	tree->root = 0;
	tree->height = 0;
	cprintf("btree_erase_node...\n");
	btree_erase_node(rootNode);
	cprintf("btree_erase_node done!\n");
	btree_unlock(tree->id);
	return 0;
    } else if (merged == 1 && rootNode->keyCount == 0) {
	struct btree_node *tempNode;

	tree->root = rootNode->children[0];

	tempNode = btree_read_node(tree, tree->root);

	btree_write_node(tempNode);
	btree_destroy_node(tempNode);

	btree_erase_node(rootNode);

	tree->height = tree->height - 1;

	btree_unlock(tree->id);
	return 0;
    }
    btree_destroy_node(rootNode);
    btree_unlock(tree->id);
    return 0;
}
