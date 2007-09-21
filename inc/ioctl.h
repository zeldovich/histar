#ifndef JOS_INC_IOCTL_H
#define JOS_INC_IOCTL_H

int jos_ioctl(struct Fd *fd, uint64_t req, va_list va);

#endif
