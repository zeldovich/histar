#ifndef LOG_H_
#define LOG_H_

#include <kern/btree.h>
#include <machine/types.h>

// max pages for log buffer
#define LOG_MEMORY          100

int log_write(struct btree_node *node)
    __attribute__ ((warn_unused_result));
int log_try_read(offset_t byteoff, void *page)
    __attribute__ ((warn_unused_result));

int64_t log_flush(void)
    __attribute__ ((warn_unused_result));
int log_apply_mem(void)
    __attribute__ ((warn_unused_result));
int log_apply_disk(uint64_t flushed_pages)
    __attribute__ ((warn_unused_result));

void log_init(void);
void log_free(offset_t off);

#endif /*LOG_H_*/
