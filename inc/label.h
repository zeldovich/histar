#ifndef JOS_INC_LABEL_H
#define JOS_INC_LABEL_H

struct ulabel {
    uint32_t ul_size;
    uint32_t ul_default;

    uint32_t ul_nent;
    uint64_t *ul_ent;
};

#define LB_HANDLE(ent)		((ent) & 0x1fffffffffffffffUL)
#define LB_LEVEL(ent)		((ent) >> 61)

#define LB_LEVEL_STAR		4
#define LB_CODE(h, level)	((h) | (((uint64_t) (level)) << 61))

#endif
