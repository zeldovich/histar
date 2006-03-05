#ifndef LOG_H_
#define LOG_H_

#include <lib/btree/btree.h>
#include <inc/queue.h>
#include <kern/freelist.h>
#include <inc/types.h>

#define LOG_COMPACT_MEM 50 

int log_apply(void)
    __attribute__ ((warn_unused_result));
int log_write(struct btree_node *node)
    __attribute__ ((warn_unused_result));
int log_flush(void)
    __attribute__ ((warn_unused_result));
int log_compact(void)
    __attribute__ ((warn_unused_result));
int log_node(offset_t byteoff, void *page)
    __attribute__ ((warn_unused_result));
void log_reset(void) ;
void log_init(uint64_t pageoff, uint64_t npages, uint64_t max_mem) ;

void log_free(offset_t off) ;

void dlog_print(void) ;
void dlog_init(void) ;

#endif /*LOG_H_*/
