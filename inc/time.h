#ifndef JOS_INC_TIME_H
#define JOS_INC_TIME_H

struct time_of_day_seg {
    uint64_t unix_nsec_offset;
};

uint64_t jos_time_nsec(void);

#endif
