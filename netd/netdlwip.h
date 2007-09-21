#ifndef JOS_INC_NETDLWIP_H
#define JOS_INC_NETDLWIP_H

/* server */
typedef enum {
    netd_if_jif,
    netd_if_tun,
} netd_dev_type;

void netd_lwip_dispatch(struct netd_op_args *a);
void netd_lwip_init(void (*cb)(void*), void *cbarg,
		    netd_dev_type type, void *if_state,
		    uint32_t ipaddr, uint32_t netmask, uint32_t gw)
     __attribute__((noreturn));


/* client */
int netd_lwip_call(struct Fd *fd, struct netd_op_args *a);
int netd_lwip_client_init(struct cobj_ref *gate, struct cobj_ref *fast_gate);
int netd_lwip_ioctl(struct netd_op_ioctl_args *a);
int netd_lwip_probe(struct Fd *fd, struct netd_op_probe_args *a);
int netd_lwip_statsync(struct Fd *fd, struct netd_op_statsync_args *a);

#endif
