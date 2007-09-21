#include <btree/btree_debug.h>
#include <btree/btree_node.h>
#include <btree/btree_utils.h>
#include <kern/lib.h>

static void
btree_leaf_count1(struct btree *tree, offset_t root, uint64_t * count)
{
    struct btree_node *root_node = btree_read_node(tree, root);

    if (BTREE_IS_LEAF(root_node))
	*count = *count + 1;
    else {
	for (int i = 0; i <= root_node->keyCount; i++) {
	    btree_leaf_count1(tree, root_node->children[i], count);
	}
    }
    btree_destroy_node(root_node);
}

static uint64_t
btree_leaf_count2(struct btree *tree)
{
    uint64_t count = 0;
    offset_t next_off = tree->left_leaf;
    struct btree_node *node;

    while (next_off) {
	node = btree_read_node(tree, next_off);
	count++;
	next_off = *btree_value(node, node->keyCount);
	btree_destroy_node(node);
    }

    return count;
}

static uint64_t
btree_size_calc(struct btree *tree)
{
    uint64_t size = 0;
    offset_t next_off = tree->left_leaf;
    struct btree_node *node;

    while (next_off) {
	node = btree_read_node(tree, next_off);
	size += node->keyCount;
	next_off = *btree_value(node, node->keyCount);
	btree_destroy_node(node);
    }

    return size;
}

static void
btree_integrity_check(struct btree *tree, offset_t root)
{
    struct btree_node *root_node = btree_read_node(tree, root);

    if (root_node->keyCount == 0)
	panic("node %"PRIu64": keyCount is 0", root);

    if (!BTREE_IS_LEAF(root_node)) {
	for (int i = 0; i <= root_node->keyCount; i++) {
	    if (root_node->children[i] == 0)
		panic("node %"PRIu64": child is 0 when it shouldn't be", root);
	}
    }

    btree_destroy_node(root_node);
}

void
btree_sanity_check_impl(void *tree)
{
    struct btree *btree = (struct btree *) tree;

    uint64_t count1 = 0;
    if (btree->root)
	btree_leaf_count1(btree, btree->root, &count1);

    uint64_t count2 = btree_leaf_count2(btree);

    if (count1 != count2)
	panic("btree_sanity_check: count mismatch: %"PRIu64" %"PRIu64"", count1,
	      count2);

    uint64_t size1 = btree->size;
    uint64_t size2 = btree_size_calc(btree);

    if (size1 != size2)
	panic("btree_sanity_check: size mismatch: %"PRIu64" %"PRIu64"", size1, size2);

    if (btree->root)
	btree_integrity_check(btree, btree->root);
}

static void
__btree_pretty_print(struct btree *tree, offset_t rootOffset, int i)
{
    int j;
    struct btree_node *rootNode;

    if (rootOffset == 0) {
	cprintf("[ empty ]\n");
	return;
    }

    rootNode = btree_read_node(tree, rootOffset);

    for (j = i; j > 0; j--)
	cprintf("    ");

    cprintf("%s [.", BTREE_IS_LEAF(rootNode) ? "L" : "N");

    for (j = 0; j < rootNode->keyCount; j++) {
	const offset_t *off = btree_key(rootNode, j);
	cprintf(" %016"PRIx64, off[0]);
	for (int k = 1; k < tree->s_key; k++)
	    cprintf("|%016"PRIx64, off[k]);
	cprintf(" .");
    }

    if (BTREE_IS_LEAF(rootNode))
	for (j = BTREE_LEAF_ORDER(rootNode) - rootNode->keyCount; j > 1; j--)
	    cprintf(" _____ .");
    else
	for (j = tree->order - rootNode->keyCount; j > 1; j--)
	    cprintf(" _____ .");

    cprintf("] - %016"PRIx64"\n", rootOffset);

    if (BTREE_IS_LEAF(rootNode)) {
	btree_destroy_node(rootNode);
	return;
    }

    for (j = 0; j <= rootNode->keyCount; j++)
	__btree_pretty_print(tree, rootNode->children[j], i + 1);

    btree_destroy_node(rootNode);
}

void
btree_pretty_print_impl(void *tree)
{
    struct btree *btree = (struct btree *) tree;
    __btree_pretty_print(btree, btree->root, 0);
}
