#ifndef JOS_KERN_EMBEDBIN_H
#define JOS_KERN_EMBEDBIN_H

#include <machine/types.h>

struct embed_bin {
    const char *name;
    const uint8_t *buf;
    uintptr_t size;
};

extern struct embed_bin embed_bins[];

#endif
