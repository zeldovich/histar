#ifndef UCLIBC_JOS64_UDSGATE_H
#define UCLIBC_JOS64_UDSGATE_H

int uds_gate_new(struct Fd *fd, uint64_t ct, struct cobj_ref *gt);
int uds_gate_accept(struct Fd *fd);
int uds_gate_connect(struct Fd *fd, struct cobj_ref gt);

#endif
