#ifndef BTREE_KERN_H_
#define BTREE_KERN_H_

#include <btree/btree_utils.h>
#include <inc/types.h>

int pbtree_open_node(uint64_t id, offset_t offset, void ** mem)
    __attribute__ ((warn_unused_result));
int pbtree_close_node(uint64_t id, offset_t offset)
    __attribute__ ((warn_unused_result));
int pbtree_save_node(struct btree_node *node)
    __attribute__ ((warn_unused_result));
int pbtree_new_node(uint64_t id, void ** mem, uint64_t * off, void *arg)
    __attribute__ ((warn_unused_result));
int pbtree_free_node(uint64_t id, offset_t offset, void *arg)
    __attribute__ ((warn_unused_result));
int frm_free(uint64_t id, offset_t offset, void *arg)
    __attribute__ ((warn_unused_result));
int frm_new(uint64_t id, void ** mem, uint64_t * off, void *arg)
    __attribute__ ((warn_unused_result));

#endif /*BTREE_KERN_H_ */
