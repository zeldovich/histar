#include <btree/btree.h>
#include <btree/btree_node.h>
#include <btree/btree_manager.h>
#include <kern/lib.h>

static void
btree_setup_node_ptrs(struct btree_node *n)
{
    n->children = (offset_t *) (&n[1]);
    n->keys = (uint64_t *) (&n->children[n->tree->order]);
}

static void
btree_init_node(struct btree_node *n, struct btree *t, uint64_t off)
{
    memset(n, 0, sizeof(struct btree_node));

    n->block.offset = off;
    n->tree = t;
    btree_setup_node_ptrs(n);

    memset(n->children, 0, sizeof(offset_t) * t->order);
    memset((void *) n->keys, 0,
	   sizeof(uint64_t) * (t->order - 1) * (t->s_key));
}

struct btree_node *
btree_new_node(struct btree *tree)
{
    int r;
    void *n;
    uint64_t off;
    if ((r = btree_alloc_node(tree->id, &n, &off)) < 0)
	panic("btree_new_node: unable to alloc mem: %s", e2s(r));

    struct btree_node *node = (struct btree_node *) n;
    btree_init_node(node, tree, off);

    return node;
}

void
btree_destroy_node(struct btree_node *node)
{
    int r;

    if (node->tree == 0 && node->block.offset == 0)
	panic("btree_destroy_node: invalid node 0x%p", node);

    struct btree *tree = node->tree;

    if ((r = btree_close_node(tree->id, node->block.offset)) < 0)
	panic("btree_destroy_node: unable to close node(%"PRIx64"): %s", 
	      node->block.offset, e2s(r));
}

struct btree_node *
btree_read_node(struct btree *tree, offset_t offset)
{
    void *mem;
    int r;

    if ((r = btree_open_node(tree->id, offset, &mem)) < 0)
	panic("btree_read_node: unable to read node(%"PRIx64"): %s", offset, e2s(r));

    struct btree_node *n = (struct btree_node *) mem;
    n->tree = tree;
    btree_setup_node_ptrs(n);

    return n;
}

offset_t
btree_write_node(struct btree_node * node)
{
    int r;

    struct btree *tree = node->tree;

    if ((r = btree_save_node(tree->id, node)) < 0)
	panic("btree_write_node: unable to write node(%"PRIx64"): %s", 
	      node->block.offset, e2s(r));

    return node->block.offset;
}

void
btree_erase_node(struct btree_node *node)
{
    int r;
    struct btree *tree = node->tree;

    if ((r = btree_free_node(tree->id, node->block.offset)) < 0)
	panic("btree_erase_node: unable to free node(%"PRIx64"): %s", 
	      node->block.offset, e2s(r));
}
