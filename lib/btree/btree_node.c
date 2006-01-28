/**
 * @file btree_node.c Node functions
 * 
 * $Id: btree_node.c,v 1.23 2002/06/23 10:28:05 chipx86 Exp $
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

#include <lib/btree/btree.h>
#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/assert.h>

struct btree_node *
bt_new_node(struct btree *tree)
{
	struct btree_node *node ; 
	if (tree->mm->alloc(tree, &node, tree->mm->arg) < 0)
		panic("bt_new_node: unable to alloc node") ;
		
	return node;
}

void
bt_destroy_node(struct btree_node * node)
{
	if (node == NULL)
		return ;
		
	struct btree *tree = node->tree ;
	
	// XXX: fix the pin interface thing...
	
	if (tree && tree->mm)
		tree->mm->pin_is(tree->mm->arg, node->block.offset, 0) ;
}

struct btree_node *
bt_read_node(struct btree *tree, offset_t offset)
{
	struct btree_node *n ;
	
	if (tree->mm) {
		if (tree->mm->node(tree, offset, &n, tree->mm->arg) == 0)
			return n ;
	}
	
	panic("bt_read_node: unable to read node %ld\n", offset) ;
	
	return NULL ;
}

offset_t
bt_write_node(struct btree_node *node)
{
	if (node == NULL)
		return 0;

	return node->block.offset;
}

void
bt_erase_node(struct btree_node *node)
{
	struct btree *tree = node->tree ;
	
	if (tree->mm)
		tree->mm->free(tree->mm->arg, node->block.offset) ;
}
