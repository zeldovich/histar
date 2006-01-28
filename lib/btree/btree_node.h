/**
 * @file btree_node.h Node functions
 * 
 * $Id: btree_node.h,v 1.10 2002/04/07 18:29:40 chipx86 Exp $
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
#ifndef _BTREE_NODE_H_
#define _BTREE_NODE_H_

#include <lib/btree/btree.h>



/**
 * Reads a B+Tree node from a buffer.
 *
 * This is meant to be called by the block functions. Don't call this
 * directly.
 *
 * @param block  The block.
 * @param buffer The buffer to read from.
 * @param extra  The parent BTree structure.
 *
 * @return A BTreeNode, or NULL on error.
 */
//void *btreeReadNodeBlock(GdbBlock *block, const char *buffer, void *extra);

/**
 * Writes a B+Tree node to a buffer.
 *
 * This is meant to be called by the block functions. Don't call this
 * directly.
 *
 * @param block  The block.
 * @param buffer The returned buffer.
 * @param size   The returned buffer size.
 */
//void btreeWriteNodeBlock(GdbBlock *block, char **buffer, unsigned long *size);

/**
 * Creates a B+Tree node block.
 *
 * This is meant to be called by the block functions. Don't call this
 * directly.
 *
 * @param block The block.
 * @param extra The parent BTree structure.
 *
 * @return A BTreeNode structure, or NULL on error.
 */
//void *btreeCreateNodeBlock(GdbBlock *block, void *extra);

/**
 * Destroys a BTreeNode structure in memory.
 * 
 * This is meant to be called by the block functions. Don't call this
 * directly.
 *
 * @param node The node to destroy.
 */
void btreeDestroyNodeBlock(void *tree);

/**
 * Creates a new BTreeNode structure.
 *
 * @param tree The B+Tree.
 *
 * @return A new BTreeNode structure.
 */
struct btree_node * bt_new_node(struct btree *tree);

/**
 * Destroys a BTreeNode structure.
 *
 * @param node The node to destroy.
 */
void bt_destroy_node(struct btree_node * node);


/**
 * Reads a node from the specified offset.
 *
 * @param tree   The active B+Tree.
 * @param offset The offset of the node to read in.
 *
 * @return The node.
 */
struct btree_node *bt_read_node(struct btree * tree, offset_t offset) ;

/**
 * Writes a node to disk.
 *
 * @param node The node to write.
 *
 * @return The offset the node was written to.
 */
offset_t bt_write_node(struct btree_node *node);

/**
 * Erases a node from disk.
 *
 * @param node The node to erase.
 */
void bt_erase_node(struct btree_node *node);

#endif /* _BTREE_NODE_H_ */
