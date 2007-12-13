#ifndef JOS_INC_FBCONS_H
#define JOS_INC_FBCONS_H

#include <inc/jthread.h>

struct fbcons_seg {
    jthread_mutex_t mu;
    volatile uint64_t updates;
    volatile uint64_t stopped;
    volatile uint64_t redraw;

    uint64_t grant;
    uint64_t taint;

    uint32_t cols;
    uint32_t rows;

    volatile uint32_t xpos;
    volatile uint32_t ypos;

    volatile uint32_t data[];
};

#endif
