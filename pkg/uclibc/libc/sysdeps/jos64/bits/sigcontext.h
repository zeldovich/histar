#ifndef UCLIBC_JOS64_SIGCONTEXT_H
#define UCLIBC_JOS64_SIGCONTEXT_H

#include <inc/utrap.h>

struct sigcontext {
    struct UTrapframe sc_utf;
};

#endif
