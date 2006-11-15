#include <btree/btree.h>
#include <btree/btree_node.h>
#include <btree/btree_manager.h>
#include <kern/lib.h>

#define CENT_NODE(ent) ((struct btree_node *)ent)
#define CENT_CHILDREN(ent) \
    ((offset_t *)((uint8_t *)ent + sizeof(struct btree_node)))
#define CENT_KEYS(ent, order) \
    ((uint64_t *)((uint8_t *)ent + sizeof(struct btree_node) + \
    sizeof(offset_t) * order))

static void
btree_init_node(struct btree_node *n, struct btree *t, uint64_t off)
{
    memset(n, 0, sizeof(struct btree_node));

    // setup pointers in node
    n->children = CENT_CHILDREN(n);
    n->keys = CENT_KEYS(n, t->order);
    n->block.offset = off;
    n->tree = t;

    memset(n->children, 0, sizeof(offset_t) * t->order);
    memset((void *) n->keys, 0,
	   sizeof(uint64_t) * (t->order - 1) * (t->s_key));
}

struct btree_node *
btree_new_node(struct btree *tree)
{
    int r;
    struct btree_node *node;
    uint8_t *n;
    uint64_t off;
    if ((r = btree_alloc_node(tree->id, &n, &off)) < 0)
	panic("btree_new_node: unable to alloc mem: %s", e2s(r));


    node = (struct btree_node *) n;
    btree_init_node(node, tree, off);

    return node;
}

void
btree_destroy_node(struct btree_node *node)
{
    int r;

    if (node->tree == 0 && node->block.offset == 0)
	panic("btree_destroy_node: invalid node 0x%lx",
	      (uint64_t)node);

    struct btree *tree = node->tree;

    if ((r = btree_close_node(tree->id, node->block.offset)) < 0)
	panic("btree_destroy_node: unable to close node(%lx): %s", 
	      node->block.offset, e2s(r));
}

struct btree_node *
btree_read_node(struct btree *tree, offset_t offset)
{
    struct btree_node *n;
    uint8_t *mem;
    int r;

    if ((r = btree_open_node(tree->id, offset, &mem)) < 0)
	panic("btree_read_node: unable to read node(%lx): %s", offset, e2s(r));

    n = (struct btree_node *) mem;
    n->children = CENT_CHILDREN(n);
    n->keys = CENT_KEYS(n, tree->order);
    n->tree = tree;

    return n;
}

offset_t
btree_write_node(struct btree_node * node)
{
    int r;

    struct btree *tree = node->tree;

    if ((r = btree_save_node(tree->id, node)) < 0)
	panic("btree_write_node: unable to write node(%lx): %s", 
	      node->block.offset, e2s(r));

    return node->block.offset;
}

void
btree_erase_node(struct btree_node *node)
{
    int r;
    struct btree *tree = node->tree;

    if ((r = btree_free_node(tree->id, node->block.offset)) < 0)
	panic("btree_erase_node: unable to free node(%lx): %s", 
	      node->block.offset, e2s(r));
}
