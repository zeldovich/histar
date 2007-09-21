#ifndef JOS_INC_BIPIPE_H
#define JOS_INC_BIPIPE_H

int bipipe(int type, int fv[2]);
int bipipe_fd(struct jcomm_ref jr, int fd_mode, uint64_t grant, uint64_t taint);

#endif
