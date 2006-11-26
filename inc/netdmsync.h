#ifndef JOS_INC_NETDMSYNC_H
#define JOS_INC_NETDMSYNC_H

#include <inc/multisync.h>
#include <inc/label.h>

int netd_new_sel_seg(uint64_t ct, int sock, struct ulabel *l, 
		     struct cobj_ref *seg);
void netd_free_sel_seg(struct cobj_ref *ss);

int netd_wstat(struct cobj_ref *ss, dev_probe_t probe, struct wait_stat *wstat);

#endif
