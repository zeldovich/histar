#ifndef JOS_INC_BIPIPE_H
#define JOS_INC_BIPIPE_H

#include <inc/fd.h>

int bipipe(int fv[2]);

// Defined in bipipe.h so pt can reuse structure and code.
enum { bipipe_bufsz = 4000 };

struct one_pipe {
    char buf[bipipe_bufsz];

    char reader_waiting;
    char writer_waiting;
    char open;
    uint32_t read_ptr;  /* read at this offset */
    uint64_t bytes; /* # bytes in circular buffer */
    pthread_mutex_t mu;
    // XXX for pt
    uint64_t ref;
};

struct bipipe_seg {
    struct one_pipe p[2];
    // XXX for pt
    uint64_t taint;
    uint64_t grant;
};

#endif
