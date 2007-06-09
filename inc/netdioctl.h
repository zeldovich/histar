#ifndef JOS_INC_NETDIOCTL_H
#define JOS_INC_NETDIOCTL_H

struct netd_ioctl_gifconf {
    char name[16];
    struct netd_sockaddr_in addr;
};

struct netd_ioctl_gifflags {
    char name[16];
    int16_t flags;
};

struct netd_ioctl_gifbrdaddr {
    char name[16];
    struct netd_sockaddr_in baddr;
};

struct netd_ioctl_args {
    uint64_t libc_ioctl;
    int size;
    int rval;
    int rerrno;
    
    union {
	struct netd_ioctl_gifconf gifconf;
	struct netd_ioctl_gifflags gifflags;
	struct netd_ioctl_gifbrdaddr gifbrdaddr;
    };
};

int netd_ioctl(struct Fd *fd, struct netd_ioctl_args *a);

int netd_lwip_ioctl(struct netd_ioctl_args *a);
int netd_linux_ioctl(struct Fd *fd, struct netd_ioctl_args *a);

#endif
