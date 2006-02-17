#ifndef LOG_H_
#define LOG_H_

#include <lib/btree/btree.h>
#include <inc/queue.h>
#include <kern/freelist.h>
#include <inc/types.h>

#define LOG_COMPACT_MEM 50 

int log_apply(void) ;
int log_write(struct btree_node *node) ;
int log_flush(void) ;
int log_compact(void) ;
int log_node(offset_t offset, void *page) ;
void log_reset(void) ;
void log_init(uint64_t off, uint64_t npages, uint64_t max_mem) ;

void dlog_print(void) ;
void dlog_init(void) ;

#endif /*LOG_H_*/
