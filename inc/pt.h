#ifndef JOS_INC_PT_H
#define JOS_INC_PT_H

#include <termios/kernel_termios.h>

int ptm_open(struct cobj_ref master_gt, struct cobj_ref slave_gt, int flags);
int pts_open(struct cobj_ref slave_gt, struct cobj_ref seg, int flags);

typedef struct __kernel_termios pt_termios;

#endif
