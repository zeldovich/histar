#ifndef JOS_INC_FBCONS_H
#define JOS_INC_FBCONS_H

#include <inc/jthread.h>

struct fbcons_seg {
    jthread_mutex_t mu;
    uint64_t updates;

    uint64_t grant;
    uint64_t taint;

    uint32_t cols;
    uint32_t rows;

    uint32_t xpos;
    uint32_t ypos;

    uint8_t  data[];
};

#endif
