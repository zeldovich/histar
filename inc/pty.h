#ifndef JOS_INC_PTY_H
#define JOS_INC_PTY_H

#include <termios/kernel_termios.h>
#include <sys/ioctl.h>
#include <inc/atomic.h>

struct pty_seg {
    struct __kernel_termios ios;
    struct winsize winsize;
    pid_t pgrp;
    jos_atomic_t ref;
};

#endif
