#ifndef JOS_INC_NETDEVENT_H
#define JOS_INC_NETDEVENT_H

#include <inc/fd.h>

int netd_probe(struct Fd *fd, dev_probe_t probe);
int netd_wstat(struct Fd *fd, dev_probe_t probe, struct wait_stat *wstat);
int netd_slow_probe(struct Fd *fd, dev_probe_t probe);

#endif
