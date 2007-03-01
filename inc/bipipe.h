#ifndef JOS_INC_BIPIPE_H
#define JOS_INC_BIPIPE_H

#include <inc/fd.h>

int bipipe(int fv[2]);

int bipipe_alloc(uint64_t container, struct cobj_ref *seg,
		 const struct ulabel *label, const char *name);
int bipipe_fd(struct cobj_ref seg, int mode, int a, 
	      uint64_t grant, uint64_t taint);

// Defined in bipipe.h so pty can reuse structure and code.
enum { bipipe_bufsz = 4000 };

struct one_pipe {
    char buf[bipipe_bufsz];

    char reader_waiting;
    char writer_waiting;
    char open;
    uint32_t read_ptr;  /* read at this offset */
    uint64_t bytes; /* # bytes in circular buffer */
    jthread_mutex_t mu;
};

struct bipipe_seg {
    struct one_pipe p[2];
};

#endif
