#ifndef LOG_H_
#define LOG_H_

#include <lib/btree/btree.h>
#include <inc/queue.h>
#include <kern/freelist.h>
#include <inc/types.h>

LIST_HEAD(node_list, btree_node);

int log_apply(void) ;
int log_write(struct btree_node *node) ;
int log_flush(void) ;
int log_node(offset_t offset, struct btree_node **store) ;
void log_init(uint64_t off, uint64_t npages, uint64_t max_mem) ;

#endif /*LOG_H_*/
