#ifndef JOS_KERN_MLT_H
#define JOS_KERN_MLT_H

#include <machine/types.h>
#include <kern/kobjhdr.h>
#include <inc/mlt.h>

struct Mlt {
    struct kobject_hdr mt_ko;
};

struct mlt_entry {
    struct Label me_l;
    uint8_t me_inuse;
    uint8_t me_buf[MLT_BUF_SIZE];
};

int  mlt_alloc(struct Label *l, struct Mlt **mtp);

// store using cur_thread's label
int  mlt_put(const struct Mlt *mlt, uint8_t *buf);

// get the first matching entry
int  mlt_get(const struct Mlt *mlt, uint8_t *buf);

#endif
