#ifndef JOS_INC_PT_H
#define JOS_INC_PT_H

int ptm_open(struct cobj_ref master_gt, struct cobj_ref slave_gt, int flags);
int pts_open(struct cobj_ref slave_gt, struct cobj_ref seg, int flags);

int pt_pts_no(struct Fd *fd, int *ptyno);

#endif
