#ifndef JOS_INC_LABEL_H
#define JOS_INC_LABEL_H

struct ulabel {
    uint32_t ul_size;
    uint32_t ul_default;

    uint32_t ul_nent;
    uint64_t *ul_ent;
};

#endif
