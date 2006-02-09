#ifndef LOG_H_
#define LOG_H_

#include <lib/btree/btree.h>
#include <inc/queue.h>
#include <kern/freelist.h>
#include <inc/types.h>

LIST_HEAD(node_list, btree_node);

int log_write(struct btree_node *node) ;
int log_flush(void) ;
void log_init(uint64_t off, uint64_t npages) ;
int log_node(offset_t offset, struct btree_node **store) ;
int log_apply(void) ;

#endif /*LOG_H_*/
