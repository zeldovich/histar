#ifndef JOS_INC_LABEL_H
#define JOS_INC_LABEL_H

#include <inc/types.h>
#include <inc/intmacro.h>

struct new_ulabel {
    uint32_t ul_size;
    uint32_t ul_nent;
    uint32_t ul_needed;
    uint64_t *ul_ent;
};

#define LB_SECRECY_FLAG		(UINT64(1) << 61)
#define LB_SECRECY(cat)		(!!( (cat) & LB_SECRECY_FLAG ))
#define LB_INTEGRITY(cat)	( !( (cat) & LB_SECRECY_FLAG ))

#endif
