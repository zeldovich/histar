#ifndef BTREE_KERN_H_
#define BTREE_KERN_H_

#include <lib/btree/btree_utils.h>
#include <inc/types.h>

int pbtree_open_node(uint64_t id, offset_t offset, uint8_t **mem) ;
int pbtree_close_node(uint64_t id, offset_t offset) ;
int pbtree_save_node(struct btree_node *node) ;
int pbtree_new_node(uint64_t id, uint8_t **mem, uint64_t *off, void *arg) ;
int pbtree_free_node(uint64_t id, offset_t offset, void *arg) ;
int frm_free(uint64_t id, offset_t offset, void *arg) ;
int frm_new(uint64_t id, uint8_t **mem, uint64_t *off, void *arg) ;



#endif /*BTREE_KERN_H_*/
