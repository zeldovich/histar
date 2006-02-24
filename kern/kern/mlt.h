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
    kobject_id_t me_ct;
    uint8_t me_inuse;
    uint8_t me_buf[MLT_BUF_SIZE];
};

int  mlt_alloc(const struct Label *l, struct Mlt **mtp)
    __attribute__ ((warn_unused_result));
int  mlt_gc(struct Mlt *mlt)
    __attribute__ ((warn_unused_result));

int  mlt_put(const struct Mlt *mlt, const struct Label *l,
	     uint8_t *buf, kobject_id_t *ct_id)
    __attribute__ ((warn_unused_result));
int  mlt_get(const struct Mlt *mlt, uint64_t idx, struct Label *l,
	     uint8_t *buf, kobject_id_t *ct_id)
    __attribute__ ((warn_unused_result));

#endif
