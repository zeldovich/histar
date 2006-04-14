#ifndef JOS_INC_WAIT_H
#define JOS_INC_WAIT_H

void child_add(pid_t pid, struct cobj_ref status_seg);
void child_clear(void);
void child_notify(void);

#endif
