#ifndef JOS_KERN_PART_H
#define JOS_KERN_PART_H

struct part_desc {
    // in bytes
    uint64_t pd_offset;
    uint64_t pd_size;
};

// the global partition 
extern struct part_desc the_part;

int part_init(void);

#endif
