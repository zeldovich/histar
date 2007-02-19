#ifndef JOS_INC_PTY_H
#define JOS_INC_PTY_H

#include <termios/kernel_termios.h>

struct pty_seg {
    char master_open;
    uint64_t slave_ref;
    struct __kernel_termios ios;
    pid_t pgrp;

    jthread_mutex_t mu;
};

#endif
